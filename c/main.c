#include <winsock2.h>
#include <windows.h>
#include <bthsdpdef.h>
#include <bluetoothapis.h>
#include <ws2bth.h>

#include <stdio.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#define TARGET_DEVICE_NAME "WaveTablePP"

bool wait_for_respons(SOCKET socket, uint8_t value)
{
    uint8_t buffer[1] = { 0 };
    int bytes_read = recv(socket, (char*)buffer, 1, 0);
    if (buffer[0] == value)
    {
        return true;
    }
    return false;
}

void connect_and_send_file(BTH_ADDR device_address)
{
    SOCKADDR_BTH sockaddr_bth = { 0 };
    sockaddr_bth.addressFamily = AF_BTH;
    sockaddr_bth.btAddr = device_address;
    sockaddr_bth.serviceClassId = SerialPortServiceClass_UUID;
    sockaddr_bth.port = BT_PORT_ANY;

    SOCKET bluetooth_socket = socket(AF_BTH, SOCK_STREAM, BTHPROTO_RFCOMM);
    if (bluetooth_socket == INVALID_SOCKET)
    {
        return;
    }

    printf("Connecting to device ...\n");
    if (connect(bluetooth_socket, (SOCKADDR*)&sockaddr_bth,
                sizeof(sockaddr_bth)) == SOCKET_ERROR)
    {
        return;
    }

    FILE* file = fopen("./c/processedAudio.txt", "rb");

    if (file)
    {

        fseek(file, 0, SEEK_END);
        size_t size = ftell(file);
        rewind(file);

        char* sound_buffer = calloc(size, sizeof(char));

        if (fread(sound_buffer, 1, size, file) != size)
        {
            return;
        }
        fclose(file);

        bool error = false;

        uint32_t chunk_size = 4096;
        uint32_t chunks = (uint32_t)size / chunk_size;

        printf(
            "\nStart sending data:\n\tTotal bytes to send: %zu\n\tChunk size: %u\n\tTotal chunks: %u\n",
            size, chunk_size, chunks);

        uint32_t i = 0;
        for (; i < chunks && !error; i++)
        {
            int offset = i * chunk_size;
            if (send(bluetooth_socket, sound_buffer + offset, chunk_size, 0) ==
                SOCKET_ERROR)
            {
                return;
            }
            error = !wait_for_respons(bluetooth_socket, 0x08);
        }
        uint32_t offset = i * chunk_size;
        int remainder = (int)size - (int)offset;
        if (!error && remainder > 0)
        {
            if (send(bluetooth_socket, sound_buffer + offset, remainder, 0) ==
                SOCKET_ERROR)
            {
                return;
            }
        }
        error = !wait_for_respons(bluetooth_socket, 0x06);

        if (error)
        {
            printf("\nError sending data\n");
        }
        else
        {
            printf("\nDone sending data\n");
        }
    }
    closesocket(bluetooth_socket);
}

BTH_ADDR find_device_address(void)
{
    BLUETOOTH_FIND_RADIO_PARAMS radio_params = {
        .dwSize = sizeof(BLUETOOTH_FIND_RADIO_PARAMS),
    };
    HANDLE radio_handle = NULL;
    HBLUETOOTH_RADIO_FIND radio_find =
        BluetoothFindFirstRadio(&radio_params, &radio_handle);

    if (!radio_find)
    {
        printf("Failed to find radio\n");
        return 1;
    }

    BLUETOOTH_DEVICE_SEARCH_PARAMS search_params = {
        .dwSize = sizeof(BLUETOOTH_DEVICE_SEARCH_PARAMS),
        .fReturnAuthenticated = TRUE,
        .fReturnRemembered = TRUE,
        .fReturnUnknown = TRUE,
        .fReturnConnected = TRUE,
        .fIssueInquiry = TRUE,
        .cTimeoutMultiplier = 2,
        .hRadio = radio_handle,
    };

    BLUETOOTH_DEVICE_INFO device_info = {
        .dwSize = sizeof(BLUETOOTH_DEVICE_INFO),
    };

    HBLUETOOTH_DEVICE_FIND device_find =
        BluetoothFindFirstDevice(&search_params, &device_info);

    if (!device_find)
    {
        printf("No Bluetooth devices found.\n");
        BluetoothFindRadioClose(radio_find);
        return 1;
    }

    bool device_found = false;
    do
    {
        printf("Found device: %ls\n", device_info.szName);
        if (wcscmp(device_info.szName, L"WaveTablePP") == 0)
        {
            printf("Found target device: %ls\n", device_info.szName);
            device_found = true;

            break;
        }
    } while (BluetoothFindNextDevice(device_find, &device_info));

    BluetoothFindDeviceClose(device_find);
    BluetoothFindRadioClose(radio_find);

    if (device_found)
    {
        return device_info.Address.ullLong;
    }
    return 0;
}

int main(void)
{
    BTH_ADDR device_address = find_device_address();
    if (device_address != 0)
    {
        WSADATA wsa_data;
        if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0)
        {
            return 1;
        }
        while (true)
        {
            printf("\nPress any key to send ");
            getchar();
            connect_and_send_file(device_address);
        }
        WSACleanup();
    }

    printf("\nPress any key to exit ");
    getchar();
}
