from flask import Flask, request, send_file 
from gtts import gTTS
import os
import io
import speech_recognition as sr
import google.generativeai as genai
import secret
import wave
from pydub import AudioSegment

genai.configure(api_key=secret.GEMINI_API_KEY)

model = genai.GenerativeModel("gemini-1.5-flash")

CWD = os.path.dirname(os.path.realpath(__file__))
upload_path = CWD + "/uploads"
app = Flask(__name__)
PROMPT_PARAMS = ["Tóm tắt tối đa trong 20 từ"]  # additional prompts to ensure the response length fits the ESP32 memory capability

def read_wav_to_bytesio(file_path):
    # Create a BytesIO object
    byte_io = io.BytesIO()

    # Open WAV file using the wave library
    with wave.open(file_path, 'rb') as wav_file:
        # Read the entire content of the WAV file
        byte_io.write(wav_file.readframes(wav_file.getnframes()))
    
    # Reset the pointer to the beginning of BytesIO
    byte_io.seek(0)

    return byte_io

def TextToSpeech(text):
    # convert text and return an audio object in wav format
    audio_buffer = io.BytesIO()
    try:
        tts = gTTS(text=text, lang='vi')  # Keep language as Vietnamese (change if needed)
        tts.save(upload_path + "/temp.mp3")  # Temporarily save the MP3 file
        
        # Convert MP3 to WAV with optimized parameters
        audio = AudioSegment.from_mp3(upload_path + "/temp.mp3")
        audio = audio.set_frame_rate(8000)  # Reduce sample rate to 8000 Hz
        audio = audio.set_channels(1)  # Use only 1 channel (mono)
        audio = audio.set_sample_width(1)  # Set encoding to Unsigned 8-bit PCM
        
        audio.export(audio_buffer, format="wav")  # Export in WAV format
        audio_buffer.seek(0)  # Reset pointer to the beginning of the buffer
        os.remove(upload_path + "/temp.mp3")  # Delete temporary file
        return audio_buffer
    except Exception as e:
        print("Error TextToSpeech: ", e)
        return audio_buffer  # empty buffer

def chatbot_response(prompt):
    try:
        # start_time = time.time()
        response = model.generate_content(prompt + '. '.join(PROMPT_PARAMS),
            generation_config=genai.GenerationConfig(
                max_output_tokens=1000,
                temperature=0.1,
            ))
        # end_time = time.time()

        # Print the response
        # print(response.text)
        # response_time = end_time - start_time
        # print(f"Response time: {response_time:.2f} seconds")
        return response.text
    except Exception as e:
        return "Error ChatbotResponse: " + str(e)

def SpeechToText(audio_data):
    recognizer = sr.Recognizer()
    with sr.AudioFile(audio_data) as source:
        audio = recognizer.record(source)
        try:
            text = recognizer.recognize_google(audio, language="vi-VN")  # Set the language to Vietnamese
            return text
        except sr.UnknownValueError:
            return "Error: Could not understand audio"
        except sr.RequestError as e:
            return f"Error: {e}"

def check_wav_file(file_path):
    with wave.open(file_path, 'rb') as wav_file:
        params = wav_file.getparams()
        print("Number of channels:", params.nchannels)
        print("Sample width (bytes):", params.sampwidth)
        print("Frame rate (samples per second):", params.framerate)
        print("Number of frames:", params.nframes)
        print("Compression type:", params.comptype)

@app.route('/upload', methods=['POST'])
def upload_wav():
    # Check Content-Type
    if request.content_type != 'audio/wav':
        print('Invalid Content-Type')
        return 'Invalid Content-Type', 400

    # Read raw data from request
    wav_data = request.data

    # Check if there is data
    if not wav_data:
        print('No data received')
        return 'No data received', 400
    file_path = os.path.join(upload_path, 'uploaded_audio.wav')  # Define your file name

    with open(file_path, 'wb') as wav_file:
        wav_file.write(wav_data)
    # Create a io.BytesIO object from the WAV data

    wav_file = io.BytesIO(wav_data)
    wav_file.seek(0)  # Reset pointer to the beginning of the file

    prompt = SpeechToText(wav_file)
    print("SpeechToText: " + prompt)

    response = chatbot_response(prompt)

    response_file = TextToSpeech(response)  # This function returns a WAV file object (not saved on disk)
    response_file_path = os.path.join(upload_path, 'response_audio.wav')  # Define the response file path
    if not os.path.exists(upload_path):
        os.makedirs(upload_path)
    with open(response_file_path, 'wb') as response_file_on_disk:
        response_file.seek(0)  # Reset pointer to the beginning of the file for writing
        response_file_on_disk.write(response_file.read())  # Save the response file to disk

    print("Response: " + response)

    # Add code to send response file to ESP32
    response_file.seek(0)  # Reset pointer to the beginning of the file to prepare for sending
    return send_file(response_file, mimetype='audio/wav', as_attachment=True, download_name='response_audio.wav')

    return 'File has been processed successfully', 200

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000)
