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
import javafx.scene.chart.LineChart;
import javafx.scene.chart.NumberAxis;
import javafx.scene.chart.XYChart;
import javafx.scene.control.Button;
import javafx.scene.control.Label;
import javafx.scene.input.Dragboard;
import javafx.scene.input.TransferMode;
import javafx.scene.layout.BorderPane;
import javafx.scene.layout.FlowPane;
import javafx.scene.layout.HBox;
import javafx.scene.layout.VBox;
import javafx.stage.Stage;

/* https://ants.inf.um.es/~felixgm/docencia/j2me/javadoc/jsr82/javax/bluetooth/UUID.html
 * SDP	                                0x0001	16-bit
 * RFCOMM	                            0x0003	16-bit
 * OBEX	                                0x0008	16-bit
 * HTTP	                                0x000C	16-bit
 * L2CAP	                            0x0100	16-bit
 * BNEP	                                0x000F	16-bit
 * Serial Port	                        0x1101	16-bit
 * ServiceDiscoveryServerServiceClassID	0x1000	16-bit
 * BrowseGroupDescriptorServiceClassID	0x1001	16-bit
 * PublicBrowseGroup	                0x1002	16-bit
 * OBEX Object Push Profile	            0x1105	16-bit
 * OBEX File Transfer Profile	        0x1106	16-bit
 * Personal Area Networking User	    0x1115	16-bit
 * Network Access Point	                0x1116	16-bit
 * Group Network	                    0x1117	16-bit
 * 
 */

public class App extends Application {

    private static Scene scene;
    private File currentFile;
    private String currentFileExtension;
    private RemoteDevice esp32Device;  
    private String connectionURL;     
    private WavFileProcessor wavFileProcessor = new WavFileProcessor();

    @Override
    public void start(Stage primaryStage) {
        Label dropLabel = new Label("Drop File");
        dropLabel.setStyle( 
                "-fx-font-family: 'Arial'; " +
                "-fx-font-size: 24; " +
                "-fx-text-fill: rgb(160, 160, 160); " +
                "-fx-border-color: white; " +
                "-fx-border-radius: 15; " +
                "-fx-background-radius: 15; " +
                "-fx-padding: 40; "
        );

        Button sendButton = new Button("Send File");
        sendButton.setDisable(true); 
        sendButton.setOnAction(event -> {
            if (currentFile != null && connectionURL != null) {
                new Thread(this::transferFileToDevice).start();
            }
        });


        VBox values = new VBox(dropLabel, sendButton);
        values.setAlignment(Pos.CENTER);
        values.setSpacing(20);
        FlowPane pane = new FlowPane(values);
        pane.setAlignment(Pos.CENTER);

        BorderPane borderPane = new BorderPane();
        borderPane.setCenter(pane);

        VBox root = new VBox(borderPane);
        root.setPadding(new Insets(150));
        root.setAlignment(Pos.CENTER);
        root.setStyle("-fx-font-family: 'Arial'; -fx-background-color: rgb(26, 26, 26);");

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
                wavFileProcessor.readWaveFile(currentFile);

                /* 
                HBox hBox = new HBox();
                hBox.getChildren().add(getLineChart(wavFileProcessor.shiftAvgDifference, 0));
                hBox.getChildren().add(getLineChart(wavFileProcessor.normalizedBuffer, 151));
                hBox.getChildren().add(getLineChart(wavFileProcessor.normalizedBuffer, (151 * 2)));
                hBox.getChildren().add(getLineChart(wavFileProcessor.normalizedBuffer, (151 * 3)));
                hBox.getChildren().add(getLineChart(wavFileProcessor.normalizedBuffer, (151 * 4)));
                hBox.getChildren().add(getLineChart(wavFileProcessor.normalizedBuffer, (151 * 5)));
                hBox.getChildren().add(getLineChart(wavFileProcessor.normalizedBuffer, (151 * 6)));
                hBox.getChildren().add(getLineChart(wavFileProcessor.normalizedBuffer, (151 * 7)));
                values.getChildren().add(hBox);
                */
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

    private LineChart<Number,Number> getLineChart(ArrayList<Float> values, int offset) {
        NumberAxis xAxis = new NumberAxis();
        NumberAxis yAxis = new NumberAxis();
        xAxis.setLabel("DD");
        LineChart<Number,Number> lineChart = new LineChart<Number,Number>(xAxis,yAxis);
        XYChart.Series<Number,Number> series = new XYChart.Series<>();
        for(int i = 0; i < 151*4; ++i) {
            series.getData().add(new XYChart.Data<>(i, values.get(i + offset)));
        }
        lineChart.getData().add(series);
        //lineChart.setPrefSize(20, 20);
        return lineChart;
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

                            // NOTE(Linus): 0x1101 is the standard UUID for Serail port (SSP), so it looks to see if the device has a SPP service up.
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

        StreamConnection streamConnection = null;
        try {
            // NOTE(Linus): Can take a long time to open the connection
            streamConnection = (StreamConnection) Connector.open(connectionURL);
            sendFile(streamConnection);
            System.out.println("File sent successfully.");
        } catch (Exception e) {
            e.printStackTrace();
        } finally {
            if(streamConnection != null) {
                try {
                    streamConnection.close();
                } catch (IOException e) {
                    e.printStackTrace();
                }
            }
        }
    }

    private void sendFile(StreamConnection connection) {
        try (OutputStream outStream = connection.openOutputStream()) {

            wavFileProcessor.cycleSampleCount = 151;
            byte cycleSampleCountLow = (byte)(wavFileProcessor.cycleSampleCount & 0xFF);
            byte cycleSampleCountHigh = (byte)((wavFileProcessor.cycleSampleCount >> 8) & 0xFF);
            byte[] header = {cycleSampleCountHigh, cycleSampleCountLow};
            outStream.write(header, 0, 2);
            Thread.sleep(10);
            outStream.flush();

            int cyclesToSend = Math.min(256, wavFileProcessor.normalizedBuffer.length / wavFileProcessor.cycleSampleCount);
            int bytesToSend = (wavFileProcessor.cycleSampleCount * cyclesToSend) * 2;
            outStream.write(wavFileProcessor.convertedBuffer, 0, bytesToSend);
            Thread.sleep(10);
            outStream.flush();
            
            System.out.println("File sent.");

        } catch (Exception e) {
            e.printStackTrace();
        }
    }
}
