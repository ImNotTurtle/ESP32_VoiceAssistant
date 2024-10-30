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

# Usage Instructions
- Power the ESP32 device using a micro-USB cable.
- Monitor logs via Serial Monitor (Arduino IDE), Monitor (PlatformIO), or Hercules software.
- When the log displays "Ready to record: Waiting for button press," the device is ready to record.
- Press and hold the button to start recording. Release the button after finishing the command (an LED light will indicate that recording is in progress).
- Wait briefly; the response will then play through the speaker.

# Contact and Contributions
Feel free to contact me via email: quypham26062002@gmail.com
