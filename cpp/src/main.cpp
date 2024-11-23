// #define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <windows.h>
#include <bluetoothapis.h>
#include <ws2bth.h>

#include <stdio.h>

#include <glad/glad.h>
#include <glfw/glfw3.h>

#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_opengl3.h>

#include "audio_processing.h"
#include "utils.h"

#define USE_SPP

typedef struct CharArray
{
    uint32_t size;
    uint32_t capacity;
    char* data;
} CharArray, String;

void string_append(String* string, char character)
{
    array_append(string, character);
    array_append(string, '\0');
}

void string_copy(String* string, const char* string_to_copy,
                 uint32_t string_length)
{
    string->size = 0;
    for (uint32_t i = 0; i < string_length; ++i)
    {
        array_append(string, string_to_copy[i]);
    }
    array_append(string, '\0');
}

void imgui_initialize(GLFWwindow* window)
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    // io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    // io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    //  io.ConfigViewportsNoTaskBarIcon = true;

    // ImGui::SetThemeStyle(ImGuiStyle_OverShiftedBlack);

    io.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/arial.ttf", 16.0f);
    io.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/arial.ttf", 32.0f);
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 450");
}

void imgui_begin_frame(void)
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

String prompt = { 0 };
bool parse_file = false;
bool send_disable = true;

bool imgui_update(GLFWwindow* window)
{
    ImGuiIO& io = ImGui::GetIO();
    (void)io;

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PopStyleVar(2);

    ImGuiWindowFlags window_flags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

    bool open = true;
    ImGui::Begin("Testing", &open, window_flags);

    ImVec2 window_size = ImGui::GetWindowSize();

    ImGui::PushFont(io.Fonts->Fonts[1]);

    float spacing = 20.0f;
    ImVec2 text_size = ImGui::CalcTextSize(prompt.data);
    ImVec2 button_size = ImVec2(100.0f, 30.0f);

    ImVec2 text_pos =
        ImVec2((window_size.x - text_size.x) * 0.5f,
               (window_size.y - text_size.y - spacing - button_size.y) * 0.5f);

    ImGui::SetCursorPos(text_pos);

    ImGui::Text("%s", prompt.data);

    ImGui::PopFont();

    ImVec2 button_pos = ImVec2((window_size.x - button_size.x) * 0.5f,
                               text_pos.y + text_size.y + spacing);
    ImGui::SetCursorPos(button_pos);

    bool pressed = false;
    ImGui::BeginDisabled(send_disable);
    if (ImGui::Button("Send", button_size))
    {
        pressed = true;
    }
    ImGui::EndDisabled();

    ImGui::End();

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    return pressed;
}

void drop_callback(GLFWwindow* window, int count, const char** paths)
{
    string_copy(&prompt, paths[0], (uint32_t)strlen(paths[0]));
    parse_file = true;
    for (int i = 0; i < count; i++)
    {
    }
}
#define TARGET_DEVICE_NAME L"WaveTablePP"

#ifdef USE_SPP

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

void connect_and_send_file(uint64_t device_address, const ByteArray& buffer)
{
    SOCKADDR_BTH sockaddr_bth = {};
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

    bool error = false;

    uint32_t chunk_size = 4096;
    uint32_t chunks = (uint32_t)buffer.size / chunk_size;

    printf(
        "\nStart sending data:\n\tTotal bytes to send: %u\n\tChunk size: %u\n\tTotal chunks: %u\n",
        buffer.size, chunk_size, chunks);

    uint32_t i = 0;
    for (; i < chunks && !error; i++)
    {
        int offset = i * chunk_size;
        if (send(bluetooth_socket, (char*)buffer.data + offset, chunk_size,
                 0) == SOCKET_ERROR)
        {
            return;
        }
        error = !wait_for_respons(bluetooth_socket, 0x08);
    }
    uint32_t offset = i * chunk_size;
    int remainder = (int)buffer.size - (int)offset;
    if (!error && remainder > 0)
    {
        if (send(bluetooth_socket, (char*)buffer.data + offset, remainder, 0) ==
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
    closesocket(bluetooth_socket);
}

void find_device_address(uint64_t* address)
{
    BLUETOOTH_FIND_RADIO_PARAMS radio_params = {};
    radio_params.dwSize = sizeof(BLUETOOTH_FIND_RADIO_PARAMS);

    HANDLE radio_handle = NULL;
    HBLUETOOTH_RADIO_FIND radio_find =
        BluetoothFindFirstRadio(&radio_params, &radio_handle);

    if (!radio_find)
    {
        printf("Failed to find radio\n");
        return;
    }

    BLUETOOTH_DEVICE_SEARCH_PARAMS search_params = {};
    search_params.dwSize = sizeof(BLUETOOTH_DEVICE_SEARCH_PARAMS);
    search_params.fReturnAuthenticated = TRUE;
    search_params.fReturnRemembered = TRUE;
    search_params.fReturnUnknown = TRUE;
    search_params.fReturnConnected = TRUE;
    search_params.fIssueInquiry = TRUE;
    search_params.cTimeoutMultiplier = 2;
    search_params.hRadio = radio_handle;

    BLUETOOTH_DEVICE_INFO device_info = {};
    device_info.dwSize = sizeof(BLUETOOTH_DEVICE_INFO);

    HBLUETOOTH_DEVICE_FIND device_find =
        BluetoothFindFirstDevice(&search_params, &device_info);

    if (!device_find)
    {
        printf("No Bluetooth devices found.\n");
        BluetoothFindRadioClose(radio_find);
        return;
    }

    bool device_found = false;
    do
    {
        printf("Found device: %ls\n", device_info.szName);
        if (wcscmp(device_info.szName, TARGET_DEVICE_NAME) == 0)
        {
            printf("Found target device: %ls\n", device_info.szName);
            device_found = true;

            break;
        }
    } while (BluetoothFindNextDevice(device_find, &device_info));

    BluetoothFindDeviceClose(device_find);
    BluetoothFindRadioClose(radio_find);

    *address = 0;
    if (device_found)
    {
        *address = device_info.Address.ullLong;
    }
}

#else

#include <iostream>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/base.h>

#include <winrt/Windows.Devices.Bluetooth.h>
#include <winrt/Windows.Devices.Bluetooth.Advertisement.h>
#include <winrt/Windows.Devices.Bluetooth.GenericAttributeProfile.h>

using namespace winrt;
using namespace Windows::Devices::Bluetooth;
using namespace Windows::Devices::Bluetooth::GenericAttributeProfile;
using namespace Windows::Devices::Bluetooth::Advertisement;
using namespace Windows::Foundation;

#define SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

bool address_found = false;

void find_device_address(uint64_t* address)
{
    BluetoothLEAdvertisementWatcher watcher;
    watcher.ScanningMode(BluetoothLEScanningMode::Active);

    watcher.Received(
        [watcher,
         address](const BluetoothLEAdvertisementWatcher&,
                  const BluetoothLEAdvertisementReceivedEventArgs& args) {
            std::wcout << L"Device Found: "
                       << args.Advertisement().LocalName().c_str() << std::endl;
            std::wcout << L"Device Address: " << args.BluetoothAddress()
                       << std::endl;

            if (args.Advertisement().LocalName() == TARGET_DEVICE_NAME)
            {
                std::wcout << L"Target device found, stopping scan..."
                           << std::endl;
                *address = args.BluetoothAddress();
                watcher.Stop();
            }
        });

    watcher.Start();
}

void connect_and_send_file(uint64_t device_address, const ByteArray& buffer)
{
    printf("Connecting\n");
    auto device =
        BluetoothLEDevice::FromBluetoothAddressAsync(device_address).get();
    auto service_result = device.GetGattServicesAsync().get();
    auto services = service_result.Services();
    GattDeviceService target_service = NULL;
    for (GattDeviceService&& service : services)
    {
        if (service.Uuid() == winrt::guid(GUID{ 0x4fafc201,
                                                0x1fb5,
                                                0x459e,
                                                { 0x8f, 0xcc, 0xc5, 0xc9, 0xc3,
                                                  0x31, 0x91, 0x4b } }))
        {
            target_service = service;
            break;
        }
    }

    auto characteristic_result = target_service.GetCharacteristicsAsync().get();
    auto characteristic = characteristic_result.Characteristics();
    GattCharacteristic target_characteristic = NULL;
    for (const GattCharacteristic& characteristic : characteristic)
    {
        if (characteristic.Uuid() ==
            winrt::guid(
                GUID{ 0xbeb5483e,
                      0x36e1,
                      0x4688,
                      { 0xb7, 0xf5, 0xea, 0x07, 0x36, 0x1b, 0x26, 0xa8 } }))
        {
            target_characteristic = characteristic;
            break;
        }
    }

    printf("Sending\n");
    uint32_t chunk_size = 512;
    uint32_t chunks = buffer.size / chunk_size;
    uint32_t remainder = buffer.size % chunk_size;
    auto write_buffer = Windows::Storage::Streams::Buffer(chunk_size);
    for (uint32_t i = 0; i < chunks; ++i)
    {
        uint32_t offset = chunk_size * i;

        write_buffer.Length(chunk_size);
        memcpy(write_buffer.data(), buffer.data + offset, chunk_size);

        auto writeResult =
            target_characteristic.WriteValueAsync(write_buffer).get();
    }
    if (remainder > 0)
    {
        uint32_t offset = chunks * chunk_size;

        write_buffer.Length(remainder);
        memcpy(write_buffer.data(), buffer.data + offset, remainder);

        auto writeResult =
            target_characteristic.WriteValueAsync(write_buffer).get();
    }
    printf("Done\n");

    device.Close();
}
#endif

int main(void)
{
#ifndef USE_SPP
    init_apartment();
#endif
    uint64_t device_address = 0;
    find_device_address(&device_address);

    if (!glfwInit())
    {
        return 1;
    }

    GLFWwindow* window = glfwCreateWindow(640, 480, "Drop File", NULL, NULL);

    if (!window)
    {
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);

    glfwSetDropCallback(window, drop_callback);
    imgui_initialize(window);

    const char* prompt_text = "Drop File";
    uint32_t prompt_len = (uint32_t)strlen(prompt_text);
    array_create(&prompt, prompt_len);
    string_copy(&prompt, prompt_text, prompt_len);

#ifdef USE_SPP
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0)
    {
        return 1;
    }
#endif

    ByteArray converted_buffer = {};
    while (!glfwWindowShouldClose(window))
    {
        if (parse_file)
        {
            converted_buffer = process_audio_buffer(prompt.data, 0);
            parse_file = false;
            send_disable = false;
        }
        imgui_begin_frame();
        if (imgui_update(window))
        {
            if(device_address != 0)
            {
                connect_and_send_file(device_address, converted_buffer);
            }
            else 
            {
                printf("Error: device address not found\n");
            }
        }
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

#ifdef USE_SPP
    WSACleanup();
#endif

    glfwTerminate();
}
