import numpy as np
import matplotlib.pyplot as plt

# Definiera tidsaxeln
time = np.linspace(0, 1, 1000)  # 1 sekund med 1000 punkter

# Gate-signal: Flera olika längder på on/off-delar
gate = np.zeros_like(time)
gate[(time > 0.1) & (time < 0.2)] = 1  # Första "on"-delen
gate[(time > 0.4) & (time < 0.7)] = 1  # Andra "on"-delen
gate[(time > 0.8) & (time < 0.85)] = 1  # Tredje "on"-delen

# Trigger-signal: Flera korta pulser
trigger = np.zeros_like(time)
trigger[(time > 0.05) & (time < 0.06)] = 1  # Första puls
trigger[(time > 0.3) & (time < 0.31)] = 1  # Andra puls
trigger[(time > 0.7) & (time < 0.71)] = 1  # Tredje puls

# CV-signal: Sinusvåg
cv_signal = 0.5 * np.sin(2 * np.pi * 3 * time) + 0.5  # Skala mellan 0 och 1

# CV-signal för V/Oct: Stegvisa nivåer med fler variationer
cv_voct = np.zeros_like(time)
cv_voct[(time >= 0.0) & (time < 0.1)] = 0.2  # Första nivå
cv_voct[(time >= 0.1) & (time < 0.2)] = 0.5  # Andra nivå
cv_voct[(time >= 0.2) & (time < 0.4)] = 0.8  # Tredje nivå
cv_voct[(time >= 0.4) & (time < 0.6)] = 0.3  # Fjärde nivå
cv_voct[(time >= 0.6) & (time < 0.8)] = 0.7  # Femte nivå
cv_voct[(time >= 0.8) & (time <= 1.0)] = 0.4  # Sjätte nivå

# Funktion för att skapa minimalistiska grafer med text
def plot_minimal_with_text(data, filename, color, text):
    plt.figure(figsize=(6, 2))
    plt.plot(time, data, color=color, linewidth=2)
    plt.axis('off')  # Ta bort allt runtom
    plt.text(0.5, -0.15, text, color=color, fontsize=12, ha='center', va='center', transform=plt.gca().transAxes)
    plt.savefig(filename, dpi=300, bbox_inches='tight', pad_inches=0)
    plt.close()

# Skapa och spara graferna med text
plot_minimal_with_text(gate, "minimal_gate_signal_multiple_with_text.png", 'black', 'Gate Signal')
plot_minimal_with_text(trigger, "minimal_trigger_signal_multiple_with_text.png", 'black', 'Trigger Signal')
plot_minimal_with_text(cv_signal, "minimal_cv_signal_with_text.png", 'black', 'CV Signal (Sine Wave)')
plot_minimal_with_text(cv_voct, "minimal_cv_voct_signal_with_text.png", 'black', 'CV Signal (V/Oct)')

print("Minimalistiska signaler med beskrivning sparade som PNG-filer.")
