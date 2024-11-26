import matplotlib.pyplot as plt
from scipy.io import wavfile

# Läs in ljudfilen
sample_rate, data = wavfile.read("audio_mono.1.wav")

# Kontrollera om ljudet är mono eller stereo
if len(data.shape) > 1:
    data = data[:, 0]  # Använd bara den första kanalen om det är stereo

# Skapa en tidsaxel (för synkronitet, även om vi inte visar den)
time = [i / sample_rate for i in range(len(data))]

# Plotta vågformen
plt.figure(figsize=(10, 4))
plt.plot(data, color='red', linewidth=1)  # Rita endast linjen
plt.axis('off')  # Ta bort axlar och allt runtom

# Spara grafen som PNG
plt.savefig("minimal_waveform_red.png", dpi=300, bbox_inches='tight', 
pad_inches=0)


