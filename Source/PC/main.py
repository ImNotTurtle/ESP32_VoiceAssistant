import socket
from flask import Flask, request, send_file
import threading
from gtts import gTTS
import os
import io
import speech_recognition as sr
import time
import google.generativeai as genai
import secret
import requests
import wave
from datetime import datetime
from pydub import AudioSegment

genai.configure(api_key=secret.GEMINI_API_KEY)

model = genai.GenerativeModel("gemini-1.5-flash")



CWD = os.path.dirname(os.path.realpath(__file__))
upload_path = CWD + "/uploads"
app = Flask(__name__)
PROMPT_PARAMS = ["Tóm tắt trong tối đa 20 từ"]

index = 0

def read_wav_to_bytesio(file_path):
    # Tạo đối tượng BytesIO
    byte_io = io.BytesIO()

    # Mở file WAV bằng thư viện wave
    with wave.open(file_path, 'rb') as wav_file:
        # Đọc toàn bộ nội dung của file WAV
        byte_io.write(wav_file.readframes(wav_file.getnframes()))
    
    # Đặt lại con trỏ về đầu của BytesIO
    byte_io.seek(0)

    return byte_io

def TextToSpeech(text): # convert text and return a audio object in wav format
    audio_buffer = io.BytesIO()
    try:
        tts = gTTS(text=text, lang='vi')
        tts.write_to_fp(audio_buffer)
        audio_buffer.seek(0)
        return audio_buffer
    except Exception as e:
        print("Error TextToSpeech: ", e)
        return audio_buffer #empty buffer


def TextToSpeech2(text):  # convert text and return an audio object in wav format
    audio_buffer = io.BytesIO()
    try:
        # Tạo âm thanh từ gTTS và lưu vào BytesIO
        tts = gTTS(text=text, lang='vi')
        tts_buffer = io.BytesIO()
        tts.save(tts_buffer)  # Lưu âm thanh vào buffer tạm
        tts_buffer.seek(0)  # Đặt con trỏ về đầu buffer
        
        # Chuyển đổi MP3 trong BytesIO thành WAV với các thông số tối ưu
        audio = AudioSegment.from_file(tts_buffer, format="mp3")
        audio = audio.set_frame_rate(11025)  # Thay đổi tốc độ lấy mẫu
        audio = audio.set_channels(1)  # Chỉ sử dụng 1 kênh (mono)
        
        # Xuất WAV vào audio_buffer
        audio.export(audio_buffer, format="wav")  # Xuất ra định dạng WAV
        audio_buffer.seek(0)  # Đặt con trỏ về đầu buffer
        return audio_buffer
    except Exception as e:
        print("Error TextToSpeech: ", e)
        return audio_buffer  # empty buffer

from gtts import gTTS
from pydub import AudioSegment
import tempfile

def TextToSpeech3(text):  # convert text and return an audio object in wav format
    audio_buffer = io.BytesIO()
    try:
        # Tạo âm thanh từ gTTS và lưu vào file tạm
        tts = gTTS(text=text, lang='vi')
        
        # Sử dụng file tạm để lưu âm thanh
        with tempfile.NamedTemporaryFile(delete=True) as tmp_file:
            tts.save(tmp_file.name)  # Lưu âm thanh vào file tạm
            
            # Chuyển đổi MP3 thành WAV
            audio = AudioSegment.from_file(tmp_file.name, format="mp3")
            audio = audio.set_frame_rate(11025)  # Tốc độ lấy mẫu
            audio = audio.set_channels(1)  # Mono
            
            # Xuất WAV vào audio_buffer
            audio.export(audio_buffer, format="wav")  # Xuất ra định dạng WAV
            audio_buffer.seek(0)  # Đặt con trỏ về đầu buffer
            return audio_buffer
    except Exception as e:
        print("Error TextToSpeech: ", e)
        return audio_buffer  # empty buffer

from pydub import AudioSegment  # Cần cài đặt pydub và ffmpeg

def TextToSpeech4(text):  # convert text and return an audio object in wav format
    audio_buffer = io.BytesIO()
    try:
        tts = gTTS(text=text, lang='vi')
        tts.save(upload_path + "/temp.mp3")  # Lưu tạm thời file MP3
        # Chuyển đổi MP3 sang WAV với các thông số tối ưu
        audio = AudioSegment.from_mp3(upload_path + "/temp.mp3")
        audio = audio.set_frame_rate(11025)  # Thay đổi tốc độ lấy mẫu
        audio = audio.set_channels(1)  # Chỉ sử dụng 1 kênh (mono)
        audio.export(audio_buffer, format="wav")  # Xuất ra định dạng WAV
        audio_buffer.seek(0)
        # os.remove("temp.mp3")  # Xóa file tạm
        return audio_buffer
    except Exception as e:
        print("Error TextToSpeech: ", e)
        return audio_buffer  # empty buffer

def TextToSpeech5(text):  # convert text and return an audio object in wav format
    audio_buffer = io.BytesIO()
    try:
        tts = gTTS(text=text, lang='vi')
        tts.save(upload_path + "/temp.mp3")  # Lưu tạm thời file MP3
        
        # Chuyển đổi MP3 sang WAV với các thông số tối ưu
        audio = AudioSegment.from_mp3(upload_path + "/temp.mp3")
        audio = audio.set_frame_rate(8000)  # Giảm tốc độ lấy mẫu xuống 8000 Hz
        audio = audio.set_channels(1)  # Chỉ sử dụng 1 kênh (mono)
        
        audio.export(audio_buffer, format="wav")  # Xuất ra định dạng WAV
        audio_buffer.seek(0)  # Đặt con trỏ về đầu buffer
        os.remove(upload_path + "/temp.mp3")  # Xóa file tạm
        return audio_buffer
    except Exception as e:
        print("Error TextToSpeech: ", e)
        return audio_buffer  # empty buffer
def TextToSpeech6(text):
    # convert text and return an audio object in wav format
    audio_buffer = io.BytesIO()
    try:
        tts = gTTS(text=text, lang='vi')
        tts.save(upload_path + "/temp.mp3")  # Lưu tạm thời file MP3
        
        # Chuyển đổi MP3 sang WAV với các thông số tối ưu
        audio = AudioSegment.from_mp3(upload_path + "/temp.mp3")
        audio = audio.set_frame_rate(8000)  # Giảm tốc độ lấy mẫu xuống 8000 Hz
        audio = audio.set_channels(1)  # Chỉ sử dụng 1 kênh (mono)
        audio = audio.set_sample_width(1)  # Đặt encoding là Unsigned 8-bit PCM
        
        audio.export(audio_buffer, format="wav")  # Xuất ra định dạng WAV
        audio_buffer.seek(0)  # Đặt con trỏ về đầu buffer
        os.remove(upload_path + "/temp.mp3")  # Xóa file tạm
        return audio_buffer
    except Exception as e:
        print("Error TextToSpeech: ", e)
        return audio_buffer  # empty buffer





def SpeechToText(audio_data):
    recognizer = sr.Recognizer()
    
    # Đọc file âm thanh từ BytesIO object
    with sr.AudioFile(audio_data) as source:
        audio = recognizer.record(source)  # Ghi lại âm thanh từ source
        
        # Chuyển đổi âm thanh thành văn bản
        try:
            text = recognizer.recognize_google(audio)  # Sử dụng Google Web Speech API
            return text
        except sr.UnknownValueError:
            return "Error SpeechToText: Speech Recognition could not understand the audio"
        except sr.RequestError as e:
            return f"Error SpeechToText: Could not request results from the speech recognition service; {e}"
        
    # common return value for not handled cases
    return ""

def chatbot_response(prompt):
    try:
        # prompt = "Sự khác biệt giữa học máy và học sâu là gì?"
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
        return "Error ChatbotResponse: " + e

def SpeechToText2(file_path):
    recognizer = sr.Recognizer()
    with sr.AudioFile(file_path) as source:
        audio = recognizer.record(source)
        try:
            text = recognizer.recognize_google(audio, language="vi-VN")  # Đặt ngôn ngữ là tiếng Việt
            return text
        except sr.UnknownValueError:
            return "Error: Could not understand audio"
        except sr.RequestError as e:
            return f"Error: {e}"
def SpeechToText3(audio_data):
    recognizer = sr.Recognizer()
    with sr.AudioFile(audio_data) as source:
        audio = recognizer.record(source)
        try:
            text = recognizer.recognize_google(audio, language="vi-VN")  # Đặt ngôn ngữ là tiếng Việt
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
    # Kiểm tra Content-Type
    if request.content_type != 'audio/wav':
        print('Content-Type không hợp lệ')
        return 'Content-Type không hợp lệ', 400

    # Đọc dữ liệu thô từ request
    wav_data = request.data

    

    # Kiểm tra xem có dữ liệu không
    if not wav_data:
        print('Không nhận được dữ liệu')
        return 'Không nhận được dữ liệu', 400
    file_path = os.path.join(upload_path, 'uploaded_audio.wav')  # Define your file name

    with open(file_path, 'wb') as wav_file:
        wav_file.write(wav_data)
    # Tạo đối tượng io.BytesIO từ dữ liệu WAV

    wav_file = io.BytesIO(wav_data)
    wav_file.seek(0)  # Đặt con trỏ về đầu file

    
    prompt = SpeechToText3(wav_file)
    print("SpeechToText: " + prompt)

    response = chatbot_response(prompt)

    response_file = TextToSpeech6(response) # Hàm này trả về object file wav (không được lưu trên ổ cứng)
    response_file_path = os.path.join(upload_path, 'response_audio.wav')  # Định nghĩa đường dẫn tệp phản hồi
    if not os.path.exists(upload_path):
        os.makedirs(upload_path)
    with open(response_file_path, 'wb') as response_file_on_disk:
        response_file.seek(0)  # Đặt con trỏ về đầu file để chuẩn bị ghi
        response_file_on_disk.write(response_file.read())  # Lưu tệp phản hồi vào ổ cứng

    print("Response: " + response)

    # thêm đoạn code gửi response file tới ESP32
    response_file.seek(0)  # Đặt con trỏ về đầu file để chuẩn bị gửi
    return send_file(response_file, mimetype='audio/wav', as_attachment=True, download_name='response_audio.wav')


    return 'File đã được xử lý thành công', 200
if __name__ == '__main__':
    # check_wav_file("D:/recording.wav")
    # # file = read_wav_to_bytesio("D:/recording.wav")
    # # text = SpeechToText(file)
    # text = SpeechToText2("D:/recording.wav")
    # print("IN: " + text)
    # response = chatbot_response(text)
    # print("\n" + "OUT: " + response)

    app.run(host='0.0.0.0', port=5000)

    pass
    # # Tạo một thread cho UDP server
    # udp_thread = threading.Thread(target=udp_server, daemon=True)
    # udp_thread.start()

    # # Chạy server Flask
    # app.run(host='0.0.0.0', port=5000)
