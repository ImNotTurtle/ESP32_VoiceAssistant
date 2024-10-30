#include <WiFi.h>
#include <WiFiUdp.h>
#include <WiFiManager.h>
#include <HTTPClient.h>
#include <Arduino.h>
#include <driver/i2s.h>
#include <freertos/FreeRTOS.h>
#include <FS.h>
#include <XT_DAC_Audio.h>
#include "esp_system.h"
#include "freertos/semphr.h"
#include <Secret.h>

// Pin definitions
#define I2S_WS 22
#define I2S_SD 26
#define I2S_SCK 21
#define DAC_PIN 25
#define RECORD_BUTTON_PIN 12
#define RECORD_LED_PIN 14
#define DAC1_PIN 25

// I2S configurations
#define I2S_PORT I2S_NUM_0
#define I2S_SAMPLE_RATE 16000 // 16kHz
#define I2S_SAMPLE_BITS 16
#define I2S_READ_LEN (16 * 1024)
#define RECORD_TIME 10 // Recording duration in seconds
#define I2S_CHANNEL_NUM 1
#define FLASH_RECORD_SIZE (I2S_CHANNEL_NUM * I2S_SAMPLE_RATE * I2S_SAMPLE_BITS / 8 * RECORD_TIME) // Max recording size is 320KB

// File system settings
#define USE_LITTLE_FS 0
#define USE_SPIFFS 1
#define USE_FILESYSTEM USE_SPIFFS

#if USE_FILESYSTEM == USE_SPIFFS
    #include <SPIFFS.h>
    #define FILESYSTEM SPIFFS
    #define FILESYSTEM_NAME "SPIFFS"
#elif USE_FILESYSTEM == USE_LITTLE_FS
    #include "LittleFS.h"
    #define FILESYSTEM MAIN_LITTLE_FS
    #define FILESYSTEM_NAME "LittleFS"
#endif

// Utilities for memory and logging
#define LOG(format, ...) (Serial.printf("%s -> " format, App_ToString(appState).c_str(), ##__VA_ARGS__))
#define LOGL(format, ...) (LOG(format "\n", ##__VA_ARGS__))         // Log with newline
#define FREE(ptr)        do { if (ptr) { delete((ptr)); (ptr) = NULL; } } while (0);

/*
Program workflow overview:
    1. Initialize file system, I2S, DAC, and RTOS tasks
    2. Connect to Wi-Fi
    3. Wait for recording trigger (press RECORD_BUTTON_PIN to start recording)
    4. Start recording with a limit of RECORD_TIME seconds
    5. Stop recording and send the recorded audio (WAV file) to the PC
    6. Receive response audio (WAV file) from the PC
    7. Play the response audio
    8. Return to step 4 for continuous operation

Notes: Recording audio is stored on the file system because the max recording size (320KB) exceeds heap capacity.
       Response audio is managed in the heap directly.
*/

enum APP_STATE
{
    SETUP,
    PAIRING,
    IDLE,
    RUNNING,
    END
};

APP_STATE appState;
File file;
const char *fileName = "/recording.wav";
XT_DAC_Audio_Class* DACAudio = NULL;
XT_Wav_Class* Sound = NULL;
bool i2sInit = false;
uint8_t* audioBuffer = NULL;
uint32_t audioSize = 0;
SemaphoreHandle_t taskSemaphore;
uint32_t heapSize = 0;
uint32_t neverUseHeapSize = 0;

void I2S_Init();
void I2S_Stop();
void I2S_DACScale(uint8_t *d_buff, uint8_t *s_buff, uint32_t len);
void Task_VoiceAssistant(void *arg);
void Server_UploadFile(const char *filePath);
void App_SetState(APP_STATE state);
APP_STATE App_GetState(void);
String App_ToString(APP_STATE state);
void File_ListFiles();
void File_WriteWavHeader(File file, uint32_t totalAudioLen);
void File_Remove(const char* path);
void Heap_Init();
void Heap_Record();
void Heap_Logging(uint8_t index = 0);
void WiFi_reconnect(void);

void setup() {
    App_SetState(SETUP);
    Serial.begin(115200);
    Serial.setDebugOutput(false);
    pinMode(RECORD_BUTTON_PIN, INPUT_PULLUP);
    pinMode(RECORD_LED_PIN, OUTPUT);
    digitalWrite(RECORD_LED_PIN, LOW);
    Heap_Init();

    if (!FILESYSTEM.begin(true)) {
        LOGL(FILESYSTEM_NAME " mount failed. Program stopping. Please reset.");
        while (1) yield();
    }
    LOGL(FILESYSTEM_NAME " mounted successfully.");
    File_Remove(fileName);
    File_ListFiles();

    I2S_Init();
    LOGL("INMP441 Microphone initialized successfully.");

    WiFi_reconnect();

    taskSemaphore = xSemaphoreCreateBinary();
    xTaskCreate(Task_VoiceAssistant, "Task_VoiceAssistant", 1024 * 3, NULL, 5, NULL);
    xSemaphoreGive(taskSemaphore);

    DACAudio = new XT_DAC_Audio_Class(DAC1_PIN, 0);
    App_SetState(IDLE);
    delay(1);
}

void loop() {
    //play sound whenever the sound is allocated
    if (Sound && !Sound->Playing) {
        if (audioBuffer) {
            DACAudio->StopAllSounds();
            FREE(Sound);
            FREE(audioBuffer);
            xSemaphoreGive(taskSemaphore);
            Heap_Logging();
        }
    }
    if (Sound) DACAudio->FillBuffer();
}

/**
 * @brief Initializes the I2S peripheral for audio input/output.
 * 
 * This function configures the I2S peripheral to prepare it for handling audio
 * data streams, enabling the necessary settings and parameters.
 */
void I2S_Init() {
    if (!i2sInit) {
        i2s_config_t i2s_config = {
            .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
            .sample_rate = I2S_SAMPLE_RATE,
            .bits_per_sample = i2s_bits_per_sample_t(I2S_SAMPLE_BITS),
            .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
            .communication_format = i2s_comm_format_t(I2S_COMM_FORMAT_STAND_I2S | I2S_COMM_FORMAT_STAND_MSB),
            .intr_alloc_flags = 0,
            .dma_buf_count = 64,
            .dma_buf_len = 1024,
            .use_apll = 1
        };
        i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
        const i2s_pin_config_t pin_config = {
            .bck_io_num = I2S_SCK,
            .ws_io_num = I2S_WS,
            .data_out_num = -1,
            .data_in_num = I2S_SD
        };
        i2s_set_pin(I2S_PORT, &pin_config);
    }
    i2sInit = true;
}

/**
 * @brief Uninstalls the I2S driver and frees associated heap memory.
 * 
 * This function stops the I2S peripheral, releases any dynamically allocated memory
 * used by the I2S driver, and prepares the system for safe shutdown of I2S operations.
 */
void I2S_Stop(){
    i2s_driver_uninstall(I2S_PORT);
    i2sInit = false;
}

/**
 * @brief Writes the WAV file header to a specified buffer.
 * 
 * This function constructs a WAV file header with the specified audio length 
 * and writes it into the given buffer.
 * 
 * @param buffer Pointer to the array where the WAV header will be stored.
 * @param totalAudioLen Total size of the audio data (in bytes) to include in the WAV header.
 */
void Buffer_WriteWavHeader(uint8_t *buffer, uint32_t totalAudioLen)
{
    uint32_t dataChunkSize = totalAudioLen * I2S_CHANNEL_NUM * (I2S_SAMPLE_BITS / 8); // Number of audio bytes (each sample has a size based on the specified bits)
    uint32_t size = 36 + dataChunkSize;

    byte wavHeader[44] = {
        'R', 'I', 'F', 'F',                                                                                                        // ChunkID "RIFF"
        (byte)(size & 0xff), (byte)((size >> 8) & 0xff), (byte)((size >> 16) & 0xff), (byte)((size >> 24) & 0xff), // ChunkSize (file size minus 8 bytes of RIFF and file size)
        'W', 'A', 'V', 'E',                                                                                                        // Format "WAVE"
        'f', 'm', 't', ' ',                                                                                                        // Subchunk1ID "fmt "
        16, 0, 0, 0,                                                                                                               // Subchunk1Size (16 bytes for PCM format)
        1, 0,                                                                                                                      // AudioFormat (1 for PCM)
        (byte)(I2S_CHANNEL_NUM & 0xff), 0,                                                                                         // NumChannels (number of audio channels, 1 for mono)
        (byte)(I2S_SAMPLE_RATE & 0xff),                                                                                            // SampleRate
        (byte)((I2S_SAMPLE_RATE >> 8) & 0xff),
        (byte)((I2S_SAMPLE_RATE >> 16) & 0xff),
        (byte)((I2S_SAMPLE_RATE >> 24) & 0xff),
        (byte)((I2S_SAMPLE_RATE * I2S_CHANNEL_NUM * (I2S_SAMPLE_BITS / 8)) & 0xff), // ByteRate = SampleRate * NumChannels * BitsPerSample/8
        (byte)(((I2S_SAMPLE_RATE * I2S_CHANNEL_NUM * (I2S_SAMPLE_BITS / 8)) >> 8) & 0xff),
        (byte)(((I2S_SAMPLE_RATE * I2S_CHANNEL_NUM * (I2S_SAMPLE_BITS / 8)) >> 16) & 0xff),
        (byte)(((I2S_SAMPLE_RATE * I2S_CHANNEL_NUM * (I2S_SAMPLE_BITS / 8)) >> 24) & 0xff),
        (byte)((I2S_CHANNEL_NUM * (I2S_SAMPLE_BITS / 8)) & 0xff), 0, // BlockAlign = NumChannels * BitsPerSample/8
        (byte)(I2S_SAMPLE_BITS & 0xff), 0,                           // BitsPerSample (number of bits per sample, 16 bits)
        'd', 'a', 't', 'a',                                          // Subchunk2ID "data"
        (byte)(dataChunkSize & 0xff),                                // Subchunk2Size (number of bytes in the data section)
        (byte)((dataChunkSize >> 8) & 0xff),
        (byte)((dataChunkSize >> 16) & 0xff),
        (byte)((dataChunkSize >> 24) & 0xff)};

    memcpy(buffer, wavHeader, 44); // Write header to buffer
}

/**
 * @brief Writes the WAV file header to a specified file.
 * 
 * This function constructs a WAV file header based on the total audio length
 * and writes it into the given file object.
 * 
 * @param file Reference to the file object where the WAV header will be written.
 * @param totalAudioLen Total size of the audio data (in bytes) to include in the WAV header.
 */
void File_WriteWavHeader(File file, uint32_t totalAudioLen)
{
    uint8_t wavHeader[44];

    Buffer_WriteWavHeader(wavHeader, totalAudioLen);

    file.seek(0);
    file.write(wavHeader, sizeof(wavHeader));
}

/**
 * @brief Removes a file from the flash memory.
 * 
 * This function deletes a specified file from the flash memory based on the provided file path.
 * 
 * @param path String containing the path to the file to be removed.
 */
void File_Remove(const char* path){
    if(FILESYSTEM.exists(path)){
        FILESYSTEM.remove(path);
    }
}

/**
 * @name Task_VoiceAssistant
 * 
 * @brief RTOS task to perform voice assistant operations including recording, saving, and processing audio data.
 * 
 * @param arg default parameter of the task
 */
void Task_VoiceAssistant(void *arg)
{
    while (true) // To loop the task continuously
    {
        if(xSemaphoreTake(taskSemaphore, portMAX_DELAY) == pdTRUE){
            App_SetState(RUNNING);
            vTaskDelay(pdMS_TO_TICKS(100)); // Allow time for initialization
            I2S_Init();
            Heap_Record();
            const unsigned int i2s_read_len = I2S_READ_LEN;
            unsigned int flash_wr_size = 0;
            size_t bytes_read = 0;
            unsigned long startTime = 0, endTime = 0;
            uint8_t *i2s_read_buff = new uint8_t[i2s_read_len];
            uint8_t* flash_wr_buff = new uint8_t[i2s_read_len];

            File_Remove(fileName);
            file = FILESYSTEM.open(fileName, FILE_WRITE, true);

            // Wait for button press to start recording
            LOGL("Ready to record: Waiting for button press");
            while (digitalRead(RECORD_BUTTON_PIN) == HIGH)
            {
                App_SetState(IDLE);
                vTaskDelay(pdMS_TO_TICKS(200));
            }
            App_SetState(RUNNING);
            // Short record to reduce latency (200ms)
            const uint8_t SHORT_RECORD_TIME = 200;
            unsigned long startRecordingTime = millis();
            while (millis() - startRecordingTime < SHORT_RECORD_TIME) {
                i2s_read(I2S_PORT, (void *)i2s_read_buff, i2s_read_len, &bytes_read, portMAX_DELAY);
            }

            LOGL("***Recording Start***");
            digitalWrite(RECORD_LED_PIN, HIGH); // Turn on LED to indicate recording
            while (flash_wr_size < FLASH_RECORD_SIZE)
            {
                i2s_read(I2S_PORT, (void *)i2s_read_buff, i2s_read_len, &bytes_read, portMAX_DELAY);
                I2S_DACScale(flash_wr_buff, (uint8_t *)i2s_read_buff, i2s_read_len);

                file.write((byte *)flash_wr_buff, i2s_read_len);

                flash_wr_size += bytes_read;
                LOGL("Sound recording %u%%", flash_wr_size * 100 / FLASH_RECORD_SIZE);

                // Stop recording if the button is released
                if (digitalRead(RECORD_BUTTON_PIN) == HIGH)
                {
                    break;
                }
            }

            LOGL("***Recording Finished***");
            digitalWrite(RECORD_LED_PIN, LOW); // Turn off LED to indicate end of recording
            
            I2S_Stop();
            Heap_Record();

            // Update WAV file size
            file.seek(0);
            File_WriteWavHeader(file, flash_wr_size);
            file.close();
            FREE(i2s_read_buff);
            FREE(flash_wr_buff);

            // Send file to server
            startTime = millis();
            Server_UploadFile(fileName);
            endTime = millis();

            LOGL("Response time: %" PRIu32 "", endTime - startTime);
            
            if(audioBuffer){
                // Free the previous sound object
                FREE(Sound);
                Sound = new XT_Wav_Class(audioBuffer);
                Sound->Speed = 0.65f;
                DACAudio->Play(Sound);
            }
        }
        else{
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
    vTaskDelete(NULL);
}

/**
 * @name I2S_DACScale
 * 
 * @brief Adjusts (scales) audio data from an I2S input buffer for DAC output.
 * 
 * This function reads 16-bit audio samples from the source buffer `s_buff`, scales them to match the
 * DAC output range, and stores the adjusted values in the destination buffer `d_buff`.
 * 
 * @param d_buff Pointer to the destination buffer where scaled DAC data will be stored.
 * @param s_buff Pointer to the source buffer containing the original 16-bit I2S audio data.
 * @param len Length of the source buffer in bytes.
 * 
 * @details
 * - The function reads two bytes (16 bits) at a time from `s_buff` to form a 12-bit audio sample.
 * - Only the 12 lowest bits of each 16-bit sample are used for DAC output.
 * - The scaled value is calculated by multiplying the sample by `256 / 2048` to adjust the
 *   amplitude range and then stored in `d_buff`.
 * - Each sample in `d_buff` is represented by two bytes where the first byte is set to 0
 *   and the second byte holds the scaled DAC value.
 */
void I2S_DACScale(uint8_t *d_buff, uint8_t *s_buff, uint32_t len)
{
    uint32_t j = 0;
    uint32_t dac_value = 0;
    for (uint32_t i = 0; i < len; i += 2)
    {
        dac_value = ((((uint16_t)(s_buff[i + 1] & 0xF) << 8) | ((s_buff[i]))));
        d_buff[j++] = 0;
        d_buff[j++] = dac_value * 256 / 2048;
    }
}

/**
 * @brief Sets the current state of the application.
 * 
 * This function updates the global variable that tracks the 
 * current state of the app to the specified state.
 * 
 * @param state The new state to set for the application.
 * @return None
 */
void App_SetState(APP_STATE state){
    appState = state;
}

/**
 * @brief Retrieves the current state of the application.
 * 
 * This function returns the current state of the app as 
 * stored in the global variable.
 * 
 * @return The current state of the application.
 */
APP_STATE App_GetState(void){
    return appState;
}

/**
 * @brief Converts the application state to a string representation.
 * 
 * This function takes an application state enum value and 
 * returns a corresponding string that indicates the state. 
 * If the state does not match any known values, it returns an 
 * empty string.
 * 
 * @param state The state of the application to convert.
 * @return A string representation of the app state.
 */
String App_ToString(APP_STATE state)
{
    switch (state)
    {
    case SETUP:
        return "Setup";
    case PAIRING:
        return "Pairing";
    case RUNNING:
        return "Running";
    case IDLE:
        return "IDLE";
    case END:
        return "End";
    }
    return "";
}

/**
 * @brief list out all files currently exist
 */
void File_ListFiles()
{
    File root = FILESYSTEM.open("/");
    File file = root.openNextFile();
    bool flag = true;
    while (file)
    {
        // Print log once
        if(flag){
            LOGL("Listing files in directory:");
            flag = false;   
        }
        LOGL("\tFILE: %s, SIZE: %d", file.name(), file.size());
        file = root.openNextFile();
    }
}

/**
 * @brief Uploads recorded audio to the server and receives the response audio.
 * 
 * This function connects to the WiFi (if not already connected) and sends a POST 
 * request to the specified server with the audio file located at the provided 
 * file path. It handles the file upload and processes the server's response, 
 * which is expected to be an audio file. The function logs the size of the 
 * uploaded file and the received response.
 * 
 * @param filePath The path to the audio file to be uploaded.
 */
void Server_UploadFile(const char *filePath)
{
    // Reconnect to WiFi if necessary
    if (WiFi.status() != WL_CONNECTED)
    {
        WiFi_reconnect();
    }
    //open audio file to read
    file = FILESYSTEM.open(filePath, FILE_READ);
    if (!file)
    {
        return;
    }
    size_t fileSize = file.size();

    //create HTTP object
    HTTPClient http;
    http.setTimeout(15000);
    http.setReuse(true);
    http.begin(serverName);
    http.addHeader("Content-Type", "audio/wav");
    
    //send file to PC
    LOGL("Sending audio to server. File size: %zu", fileSize);
    int httpResponseCode = http.sendRequest("POST", &file, fileSize);
    file.close();
    Heap_Record();

    //handle the response
    if (httpResponseCode > 0)
    {
        int contentLength = http.getSize(); // Get the size of the response

        if (contentLength > 0)
        {
            //audio file received, read the file and store to buffer
            // LOGL("Content length: %u", contentLength);
            WiFiClient *stream = http.getStreamPtr();
            FREE(audioBuffer);
            audioBuffer = new uint8_t[contentLength];
            audioSize = 0;
            Heap_Record();
            if(audioBuffer){
                while (http.connected() && audioSize < contentLength)
                {
                    size_t availableData = stream->available();
                    if (availableData > 0)
                    {
                        uint8_t buffer[512];
                        int bytesRead = stream->readBytes(buffer, min(sizeof(buffer), contentLength - audioSize));
                        if (audioSize + bytesRead <= contentLength) {
                            memcpy(audioBuffer + audioSize, buffer, bytesRead * sizeof(uint8_t));
                            audioSize += bytesRead;
                        } else {
                            break; //Stop if there is a risk of exceeding the size
                        }
                    }
                }

                LOGL("\tReceived response WAV file, size: %d bytes", audioSize);
                Buffer_WriteWavHeader(audioBuffer, audioSize);
            }
            else{
                LOGL("audioBuffer can not be allocated with size: %zu", contentLength);
            }
        }
        else
        {
            LOGL("Content length is invalid");
        }
    }
    else
    {
        LOGL("Error on HTTP request: %s", http.errorToString(httpResponseCode).c_str());
    }
    http.end();
}

/**
 * @brief Reconnects to the WiFi router.
 * 
 * This function attempts to establish a connection to the specified WiFi network using the 
 * provided SSID and password. It will retry the connection every 2 seconds until it is 
 * successfully connected, logging progress every three attempts. 
 */
void WiFi_reconnect(void) {
    WiFi.begin(ssid, password);
    uint8_t tryCount = 0;
    while (WiFi.status() != WL_CONNECTED) {
        tryCount++;
        if (tryCount % 3 == 0) {
            LOGL("WiFi connecting... Attemps: %d", tryCount / 3 + 1);
        }
        delay(2000);
    }
    LOGL("WiFi connected.");
}

/**
 * @brief Initializes heap memory tracking for the application.
 * 
 * This function captures the current available heap size at initialization and stores it in 
 * `heapSize` and `neverUseHeapSize` variables. These values are used to monitor heap memory 
 * usage throughout the application's runtime.
 */
void Heap_Init() {
    size_t freeHeap = heap_caps_get_free_size(MALLOC_CAP_DEFAULT);
    heapSize = freeHeap;
    neverUseHeapSize = heapSize;
}

/**
 * @brief Records the current available heap memory and updates the minimum free heap size.
 * 
 * This function checks the current available heap memory and updates `heapSize` with the 
 * latest free heap size. It also compares the current `heapSize` with `neverUseHeapSize` to 
 * track the minimum heap usage observed.
 */
void Heap_Record() {
    size_t freeHeap = heap_caps_get_free_size(MALLOC_CAP_DEFAULT);
    if (heapSize == 0) {
        heapSize = freeHeap;
        neverUseHeapSize = heapSize;
    } else {
        heapSize = freeHeap;
        neverUseHeapSize = min(heapSize, neverUseHeapSize);
    }
}

/**
 * @brief Logs the current heap usage and tracks the minimum free heap size.
 * 
 * This function logs the current available heap memory (`freeHeap`), the difference between 
 * the initial and current heap size, and the minimum observed heap size (`neverUseHeapSize`).
 * The `index` parameter is used to customize log labels for easier tracking of heap usage.
 * 
 * @param index The label identifier for the log entry.
 */
void Heap_Logging(uint8_t index) {
    size_t freeHeap = heap_caps_get_free_size(MALLOC_CAP_DEFAULT);
    if (index == 0) {
        LOGL("Heap Log:");
    } else {
        LOGL("Heap Log %d:", index);
    }
    LOGL("\tFree Heap Size: %zu bytes", freeHeap);
    LOGL("\tHeap Size decreased by: %zd bytes", heapSize - freeHeap);
    heapSize = freeHeap;
    neverUseHeapSize = min(heapSize, neverUseHeapSize);
    LOGL("\tMinimum free heap: %zu bytes", neverUseHeapSize);
}