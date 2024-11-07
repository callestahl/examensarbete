package com.userside;

import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.OutputStream;
import java.util.ArrayList;

import javax.bluetooth.DeviceClass;
import javax.bluetooth.DiscoveryAgent;
import javax.bluetooth.DiscoveryListener;
import javax.bluetooth.LocalDevice;
import javax.bluetooth.RemoteDevice;
import javax.bluetooth.ServiceRecord;
import javax.bluetooth.UUID;
import javax.microedition.io.Connector;
import javax.microedition.io.StreamConnection;


import javafx.application.Application;
import javafx.application.Platform;
import javafx.fxml.FXMLLoader;
import javafx.geometry.Insets;
import javafx.geometry.Pos;
import javafx.scene.Parent;
import javafx.scene.Scene;
import javafx.scene.control.Button;
import javafx.scene.control.Label;
import javafx.scene.input.Dragboard;
import javafx.scene.input.TransferMode;
import javafx.scene.layout.BorderPane;
import javafx.scene.layout.FlowPane;
import javafx.scene.layout.VBox;
import javafx.stage.Stage;

public class App extends Application {

    private static Scene scene;
    private File currentFile;
    private RemoteDevice esp32Device;  // Holds the ESP32 device to connect to later
    private String connectionURL;      // Stores connection URL for the ESP32's Serial Port Service

    @Override
    public void start(Stage primaryStage) {
        Label dropLabel = new Label("Drop File");
        dropLabel.setStyle(
                "-fx-font-size: 24; " +
                "-fx-text-fill: rgb(160, 160, 160); " +
                "-fx-border-color: white; " +
                "-fx-border-radius: 15; " +
                "-fx-background-radius: 15; " +
                "-fx-padding: 40; "
        );

        Button sendButton = new Button("Send File");
        sendButton.setDisable(true);  // Initially disable until a file is selected and device is found
        sendButton.setOnAction(event -> {
            if (currentFile != null && connectionURL != null) {
                new Thread(this::transferFileToDevice).start();
            }
        });

        VBox values = new VBox(dropLabel, sendButton);
        values.setAlignment(Pos.CENTER);
        FlowPane pane = new FlowPane(values);
        pane.setAlignment(Pos.CENTER);

        BorderPane borderPane = new BorderPane();
        borderPane.setCenter(pane);

        VBox root = new VBox(borderPane);
        root.setPadding(new Insets(150));
        root.setAlignment(Pos.CENTER);
        root.setStyle("-fx-background-color: rgb(26, 26, 26);");

        root.setOnDragOver(event -> {
            if (event.getGestureSource() != root && event.getDragboard().hasFiles()) {
                event.acceptTransferModes(TransferMode.COPY);
            }
            event.consume();
        });
        root.setOnDragDropped(event -> {
            Dragboard db = event.getDragboard();
            boolean success = false;
            if (db.hasFiles()) {
                currentFile = db.getFiles().get(0);
                dropLabel.setText("File dropped: " + currentFile.getName());
                success = true;
                sendButton.setDisable(false);  
            }
            event.setDropCompleted(success);
            event.consume();
        });

        Scene scene = new Scene(root);
        primaryStage.setScene(scene);
        primaryStage.setTitle("Drag and Drop File");
        primaryStage.show();

        Thread thread = new Thread(this::discoverBluetoothDevices);
        thread.setDaemon(true);
        thread.start();
    }

    static void setRoot(String fxml) throws IOException {
        scene.setRoot(loadFXML(fxml));
    }

    private static Parent loadFXML(String fxml) throws IOException {
        FXMLLoader fxmlLoader = new FXMLLoader(App.class.getResource(fxml + ".fxml"));
        return fxmlLoader.load();
    }

    public static void main(String[] args) {
        launch();
    }

    private void discoverBluetoothDevices() {
        try {
            LocalDevice localDevice = LocalDevice.getLocalDevice();
            DiscoveryAgent agent = localDevice.getDiscoveryAgent();
            ArrayList<RemoteDevice> devices = new ArrayList<>();

            agent.startInquiry(DiscoveryAgent.GIAC, new DiscoveryListener() {
                @Override
                public void deviceDiscovered(RemoteDevice btDevice, DeviceClass cod) {
                    devices.add(btDevice);
                    try {
                        String deviceName = btDevice.getFriendlyName(false);
                        System.out.println("Device found: " + deviceName);

                        if (deviceName.equals("WaveTablePP")) {  
                            esp32Device = btDevice;  
                            Platform.runLater(() -> System.out.println("ESP32 found. Discovering services..."));

                            agent.searchServices(null, new UUID[]{new UUID(0x1101)}, btDevice, this);
                        }

                    } catch (Exception e) {
                        System.out.println("Device found: " + btDevice.getBluetoothAddress());
                    }
                }

                @Override
                public void inquiryCompleted(int discType) {
                    System.out.println("Device Inquiry Completed.");
                    if (esp32Device == null) {
                        Platform.runLater(() -> System.out.println("ESP32 device not found."));
                    }
                }

                @Override
                public void servicesDiscovered(int transID, ServiceRecord[] servRecord) {
                    for (ServiceRecord service : servRecord) {
                        connectionURL = service.getConnectionURL(ServiceRecord.NOAUTHENTICATE_NOENCRYPT, false);
                        if (connectionURL != null) {
                            Platform.runLater(() -> System.out.println("Service found and ready for file transfer."));
                            break;
                        }
                    }
                }

                @Override
                public void serviceSearchCompleted(int transID, int respCode) {
                    if (connectionURL == null) {
                        System.out.println("SPP service not found on ESP32.");
                    }
                }
            });

            synchronized (this) {
                this.wait();  
            }

        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    private void transferFileToDevice() {
        if (connectionURL == null) {
            System.out.println("SPP service not found on ESP32. Cannot transfer file.");
            return;
        }

        try {
            // This function takes alot of time
            StreamConnection streamConnection = (StreamConnection) Connector.open(connectionURL);
            sendFile(streamConnection);
            System.out.println("File sent successfully.");
        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    private void sendFile(StreamConnection connection) {
        try (OutputStream outStream = connection.openOutputStream();
             FileInputStream fileInputStream = new FileInputStream(currentFile)) {

            byte[] buffer = new byte[1024];
            int bytesRead;
            while ((bytesRead = fileInputStream.read(buffer)) != -1) {
                outStream.write(buffer, 0, bytesRead);
                Thread.sleep(10);
            }
            outStream.flush();
            System.out.println("File sent.");

        } catch (Exception e) {
            e.printStackTrace();
        }
    }
}
