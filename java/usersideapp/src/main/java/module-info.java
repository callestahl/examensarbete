module com.userside {
    requires javafx.controls;
    requires javafx.fxml;
    requires bluecove;

    opens com.userside to javafx.fxml;
    exports com.userside;
}
