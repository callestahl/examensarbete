package com.userside;

import java.io.ByteArrayOutputStream;
import java.io.File;

import javax.sound.sampled.AudioFormat;
import javax.sound.sampled.AudioInputStream;
import javax.sound.sampled.AudioSystem;

public class WavFileProcessor {


    public void readWaveFile(File file) {
        int i = file.getName().lastIndexOf('.');
        String currentFileExtension =  file.getName().substring(i + 1);
        if(currentFileExtension.equalsIgnoreCase("wav")) {
            try {
                AudioInputStream audioInputStream = AudioSystem.getAudioInputStream(file);

                ByteArrayOutputStream bufferStream = new ByteArrayOutputStream();
                byte[] tempBuffer = new byte[4096];
                int bytesRead = 0;
                while ((bytesRead = audioInputStream.read(tempBuffer)) != -1) {
                    bufferStream.write(tempBuffer, 0, bytesRead);
                }
                byte[] audioBuffer = bufferStream.toByteArray();

                AudioFormat format = audioInputStream.getFormat();
                int channels = format.getChannels();
                int bitsPerSample = format.getSampleSizeInBits();

                float[] normalizedBuffer = null;
                if(channels == 2) {
                    normalizedBuffer = processStereoSoundBuffer(audioBuffer, bitsPerSample);
                } else if (channels == 1) {
                    normalizedBuffer = processMonoSoundBuffer(audioBuffer, bitsPerSample);
                } else {
                    System.out.printf("Channel count: %d sound format not supported\n", channels);
                }

                



                audioInputStream.close();
                bufferStream.close();
            } catch(Exception e) {
                e.printStackTrace();
            }
        }
    }

    private float[] processMonoSoundBuffer(byte[] audioBuffer, int bitsPerSample) {
        float[] normalizedBuffer = null;
        switch (bitsPerSample) {
            case 8:
                normalizedBuffer = normalize8Bit(audioBuffer);
                break;
            case 16:
                normalizedBuffer = normalize16Bit(audioBuffer);
                break;
            case 24:
                normalizedBuffer = normalize24Bit(audioBuffer);
                break;
            case 32:
                normalizedBuffer = normalize32Bit(audioBuffer);
                break;
        
            default:
                break;
        }
        return normalizedBuffer;
    }

    private float[] processStereoSoundBuffer(byte[] audioBuffer, int bitsPerSample) {
        float[][] normalizedBuffer = null;
        switch (bitsPerSample) {
            case 8:
                normalizedBuffer = normalize8BitStereo(audioBuffer);
                break;
            case 16:
                normalizedBuffer = normalize16BitStereo(audioBuffer);
                break;
            case 24:
                normalizedBuffer = normalize24BitStereo(audioBuffer);
                break;
            case 32:
                normalizedBuffer = normalize32BitStereo(audioBuffer);
                break;
        
            default:
                break;
        }
        return convertStereoToMonoSumAvg(normalizedBuffer);
    }

    private float[] convertStereoToMonoSumAvg(float[][] audioBuffer) {
        int length = audioBuffer[0].length;
        float[] convertedBuffer = new float[length];

        for(int i = 0; i < length; ++i) {
            convertedBuffer[i] = (audioBuffer[0][i] + audioBuffer[1][i]) / 2;
        }

        return convertedBuffer;
    }

    private static float[] normalize8Bit(byte[] audioBytes) {
        float[] normalizedSamples = new float[audioBytes.length];
        for (int i = 0; i < audioBytes.length; i++) {
            int unsignedSample = audioBytes[i] & 0xFF;
            normalizedSamples[i] = (unsignedSample - 128) / 128.0f;
        }
        return normalizedSamples;
    }

    private static float[] normalize16Bit(byte[] audioBytes) {
        int numSamples = audioBytes.length / 2;
        float[] normalizedSamples = new float[numSamples];
        for (int i = 0; i < numSamples; i++) {
            int low = audioBytes[2 * i] & 0xFF;
            int high = audioBytes[2 * i + 1] << 8;
            short sample = (short) (high | low);
            normalizedSamples[i] = sample / 32768.0f;
        }
        return normalizedSamples;
    }

    private static float[] normalize24Bit(byte[] audioBytes) {
        int numSamples = audioBytes.length / 3;
        float[] normalizedSamples = new float[numSamples];
        for (int i = 0; i < numSamples; i++) {
            int low = audioBytes[3 * i] & 0xFF;
            int medium = audioBytes[3 * i + 1] << 8;
            int high = audioBytes[3 * i + 2] << 16;
            short sample = (short) (high | medium | low);
            normalizedSamples[i] = sample / 8388608.0f;
        }
        return normalizedSamples;
    }

    private static float[] normalize32Bit(byte[] audioBytes) {
        int numSamples = audioBytes.length / 4;
        float[] normalizedSamples = new float[numSamples];
        for (int i = 0; i < numSamples; i++) {
            int b1 = audioBytes[4 * i] & 0xFF;
            int b2 = (audioBytes[4 * i + 1] & 0xFF) << 8;
            int b3 = (audioBytes[4 * i + 2] & 0xFF) << 16;
            int b4 = audioBytes[4 * i + 3] << 24;
            int sample = b4 | b3 | b2 | b1;
            normalizedSamples[i] = sample / 2147483648.0f;
        }
        return normalizedSamples;
    }

    private static float[][] normalize8BitStereo(byte[] audioBytes) {
        int numSamples = audioBytes.length / 2;
        float[][] normalizedSamples = new float[2][numSamples];
        for (int i = 0, j = 0; i < audioBytes.length; i += 2, j++) {
            int leftSample = audioBytes[i] & 0xFF;
            normalizedSamples[0][j] = (leftSample - 128) / 128.0f;
            int rightSample = audioBytes[i + 1] & 0xFF;
            normalizedSamples[1][j] = (rightSample - 128) / 128.0f;
        }
        return normalizedSamples;
    }

    private static float[][] normalize16BitStereo(byte[] audioBytes) {
        int numSamples = audioBytes.length / 4;
        float[][] normalizedSamples = new float[2][numSamples];
        for (int i = 0, j = 0; i < audioBytes.length; i += 4, j++) {
            int lowLeft = audioBytes[i] & 0xFF;
            int highLeft = audioBytes[i + 1] << 8;
            short leftSample = (short) (highLeft | lowLeft);
            normalizedSamples[0][j] = leftSample / 32768.0f;
            int lowRight = audioBytes[i + 2] & 0xFF;
            int highRight = audioBytes[i + 3] << 8;
            short rightSample = (short) (highRight | lowRight);
            normalizedSamples[1][j] = rightSample / 32768.0f;
        }
        return normalizedSamples;
    }

    private static float[][] normalize24BitStereo(byte[] audioBytes) {
        int numSamples = audioBytes.length / 6;
        float[][] normalizedSamples = new float[2][numSamples];
        for (int i = 0, j = 0; i < audioBytes.length; i += 6, j++) {
            int lowLeft = audioBytes[i] & 0xFF;
            int midLeft = (audioBytes[i + 1] & 0xFF) << 8;
            int highLeft = audioBytes[i + 2] << 16;
            int leftSample = highLeft | midLeft | lowLeft;
            normalizedSamples[0][j] = leftSample / 8388608.0f;
            int lowRight = audioBytes[i + 3] & 0xFF;
            int midRight = (audioBytes[i + 4] & 0xFF) << 8;
            int highRight = audioBytes[i + 5] << 16;
            int rightSample = highRight | midRight | lowRight;
            normalizedSamples[1][j] = rightSample / 8388608.0f;
        }
        return normalizedSamples;
    }

    private static float[][] normalize32BitStereo(byte[] audioBytes) {
        int numSamples = audioBytes.length / 8;
        float[][] normalizedSamples = new float[2][numSamples];
        for (int i = 0, j = 0; i < audioBytes.length; i += 8, j++) {
            int b1Left = audioBytes[i] & 0xFF;
            int b2Left = (audioBytes[i + 1] & 0xFF) << 8;
            int b3Left = (audioBytes[i + 2] & 0xFF) << 16;
            int b4Left = audioBytes[i + 3] << 24;
            int leftSample = b4Left | b3Left | b2Left | b1Left;
            normalizedSamples[0][j] = leftSample / 2147483648.0f;
            int b1Right = audioBytes[i + 4] & 0xFF;
            int b2Right = (audioBytes[i + 5] & 0xFF) << 8;
            int b3Right = (audioBytes[i + 6] & 0xFF) << 16;
            int b4Right = audioBytes[i + 7] << 24;
            int rightSample = b4Right | b3Right | b2Right | b1Right;
            normalizedSamples[1][j] = rightSample / 2147483648.0f;
        }
        return normalizedSamples;
    }



}
