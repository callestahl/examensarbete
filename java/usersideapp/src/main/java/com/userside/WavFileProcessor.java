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
                AudioFormat format = audioInputStream.getFormat();
                // format.getSampleSizeInBits();

                ByteArrayOutputStream bufferStream = new ByteArrayOutputStream();
                byte[] tempBuffer = new byte[4096];
                int bytesRead = 0;
                while ((bytesRead = audioInputStream.read(tempBuffer)) != -1) {
                    bufferStream.write(tempBuffer, 0, bytesRead);
                }
                byte[] audioBytes = bufferStream.toByteArray();

                System.out.println("Bytes read: " + audioBytes.length);

                audioInputStream.close();
                bufferStream.close();
            } catch(Exception e) {
                e.printStackTrace();
            }
        }
    }
}
