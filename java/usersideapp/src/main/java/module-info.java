module com.userside {
    requires javafx.controls;
    requires javafx.fxml;
    requires bluecove;
    requires java.desktop;
    requires javafx.graphics;

    opens com.userside to javafx.fxml;
    exports com.userside;
}
