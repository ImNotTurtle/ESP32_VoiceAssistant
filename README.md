## Voice Assistant - ESP32 Programming Personal Project

# Introduction
- **Project Overview**: This project focuses on a voice-controlled virtual assistant using GeminiAI and ESP32. For more detailed information, please refer to the documentation folder.
- **Project Goals and Applications**: The objective is to build a virtual assistant leveraging GeminiAI and the ESP32 device to enable users to perform tasks through voice commands, aiding in information retrieval and response.

# Components and Technologies Used
- **Hardware**: ESP32-WROOM-32D, PC, 8-ohm speaker, INMP441 microphone, LM386 audio amplifier circuit
- **Software**: Visual Studio Code, PlatformIO extension
- **Protocols**: HTTP

# System Workflow
1. Record audio on the ESP32
2. Send the recorded audio file to the server (PC)
3. Process the audio on the server, converting speech to text
4. Send the text to GeminiAI for a response
5. Convert the AI response text to audio on the PC
6. Send the audio response file back to the ESP32 and play it through the speaker

# Hardware Setup
For detailed setup instructions, please refer to the documentation folder.

# Software Installation Guide
- **ESP32 Programming**: Follow this link for a guide on programming the ESP32 using PlatformIO and Visual Studio Code: [YouTube Setup Guide](https://www.youtube.com/watch?app=desktop&v=nlE2203Q3XI)
- **Python programming**: Download Python and install the libraries necessary for the program: 
	+ `Python` 3.12.7
	+ `SpeechRecognition` 3.10.4
	+ `Flask` 3.0.3
	+ `pydub` 0.25.1
	+ `gTTS` 2.5.3
	+ `google-generativeai` 0.8.2

# Usage Instructions
- **Setup before running**: Check your PC's IP address (run this command on Windows Terminal: `for /f "tokens=14" %i in ('ipconfig ^| findstr /i "IPv4"') do @echo %i`), then edit the `hostIP` variable in the `secret.h` file in the `Source/ESP32/include` directory to match this IP. Also, update the Wi-Fi credentials to match your home network information. Next, obtain the API key for GeminiAI usage (visit this link: https://aistudio.google.com/app/apikey), and paste the API key into the `Secret.py` file in the `Source/PC` directory.
- First, run the Python program on the PC to receive HTTP requests from the ESP32.
- Power the ESP32 device using a micro-USB cable.
- Monitor logs via Serial Monitor (Arduino IDE), Monitor (PlatformIO), or Hercules software.
- When the log displays "Ready to record: Waiting for button press," the device is ready to record.
- Press and hold the button to start recording. Release the button after finishing the command (an LED light will indicate that recording is in progress).
- Wait briefly; the response will then play through the speaker (monitor logs to see if any error happened).

# Contact and Contributions
Feel free to contact me via email: quypham26062002@gmail.com
