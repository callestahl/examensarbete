// #define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <windows.h>
#include <bluetoothapis.h>
#include <ws2bth.h>

#include <stdio.h>
#include <time.h>

#include <glad/glad.h>
#include <glfw/glfw3.h>

#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_opengl3.h>

#include "audio_processing.h"
#include "utils.h"

#define USE_SPP

uint64_t get_time_micro(void)
{
    struct timespec now;
    timespec_get(&now, TIME_UTC);
    return ((uint64_t)now.tv_sec * 1000000UL) + (uint64_t)(now.tv_nsec * 0.001);
}

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

typedef struct ProfileData
{
    uint64_t start_time_connect;
    uint64_t end_time_connect;

    uint64_t start_time_find;
    uint64_t end_time_find;

    uint64_t start_time_sending;
    uint64_t end_time_sending;

    uint32_t time_chunk_before_count;
    uint32_t time_chunk_sent_count;

    uint64_t time_buffer_chunk_before[2048];
    uint64_t time_buffer_chunk_sent[2048];

} ProfileData;

typedef struct Profile
{
    uint64_t total_bytes;
    uint32_t chunk_size;
    uint32_t chunks;
    uint32_t remainder;

    uint32_t count;
    ProfileData* data;
} Profile;

const uint32_t buffer_size = 4096;

void profile_append_to_file(Profile* profile, const char* file_name,
                            const char* file_name_formatted,
                            uint32_t run_offset)
{
    FILE* profile_file = NULL;
    fopen_s(&profile_file, file_name, "a");
    if (!profile_file)
    {
        printf("Failed to open profile file for writing.\n");
        return;
    }

    fprintf(profile_file, "Profiling Data for connect_and_send_file:\n\n");
    fprintf(profile_file, "Total Bytes Sent: %llu\n", profile->total_bytes);
    fprintf(profile_file, "Chunk Size: %u bytes\n", profile->chunk_size);
    fprintf(profile_file, "Number of Chunks Sent: %u\n", profile->chunks);
    fprintf(profile_file, "Remaining Bytes: %d\n", profile->remainder);

    fprintf(profile_file, "\nConnection Time (microseconds): ");
    for (uint32_t i = 0; i < profile->count; ++i)
    {
        char print_format[] = "%llu, ";
        if (i == (profile->count - 1))
        {
            print_format[4] = '\0';
        }
        fprintf(profile_file, print_format,
                profile->data[i].end_time_connect -
                    profile->data[i].start_time_connect);
    }

    fprintf(profile_file,
            "\nFinding characteristic and service (microseconds): ");
    for (uint32_t i = 0; i < profile->count; ++i)
    {
        char print_format[] = "%llu, ";
        if (i == (profile->count - 1))
        {
            print_format[4] = '\0';
        }
        fprintf(profile_file, print_format,
                profile->data[i].end_time_find -
                    profile->data[i].start_time_find);
    }

    fprintf(profile_file, "\nTotal Sending Time (microseconds): ");
    for (uint32_t i = 0; i < profile->count; ++i)
    {
        char print_format[] = "%llu, ";
        if (i == (profile->count - 1))
        {
            print_format[4] = '\0';
        }
        fprintf(profile_file, print_format,
                profile->data[i].end_time_sending -
                    profile->data[i].start_time_sending);
    }

    fprintf(profile_file, "\n\nChunk Timing Details:\n");

    fprintf(profile_file, "Chunk round trip times (microseconds):\n");
    for (uint32_t i = 0; i < profile->data[0].time_chunk_before_count; i++)
    {
        fprintf(profile_file, "Chunk %u: ", i + 1);
        for (uint32_t j = 0; j < profile->count; ++j)
        {
            char print_format[] = "%llu, ";
            if (j == (profile->count - 1))
            {
                print_format[4] = '\n';
                print_format[5] = '\0';
            }
            fprintf(profile_file, print_format,
                    profile->data[j].time_buffer_chunk_sent[i] -
                        profile->data[j].time_buffer_chunk_before[i]);
        }
    }

    fprintf(profile_file, "\n----------------------------------------\n");

    fclose(profile_file);

    fopen_s(&profile_file, file_name_formatted, "a");

    fseek(profile_file, 0, SEEK_END);
    if (ftell(profile_file) == 0)
    {
        fprintf(profile_file,
                "Run,Protocol,Size,Total Sending Time (microseconds)\n");
    }

    for (uint32_t i = 0; i < profile->count; ++i)
    {
        uint64_t sending_time = profile->data[i].end_time_sending -
                                profile->data[i].start_time_sending;

        fprintf(profile_file, "%u,BLE,%llu,%llu\n", run_offset + (i + 1),
                profile->total_bytes, sending_time);
    }
    fclose(profile_file);
}

void profile_append_to_file_buffer(Profile* profile, const char* file_name,
                                   const char* file_name_formatted,
                                   const char* file_name_mean,
                                   uint32_t run_offset)
{
    FILE* profile_file = NULL;
    fopen_s(&profile_file, file_name, "a");
    if (!profile_file)
    {
        printf("Failed to open profile file for writing.\n");
        return;
    }

    fprintf(profile_file, "Profiling Data for connect_and_send_file:\n\n");
    fprintf(profile_file, "Total Bytes Sent: %llu\n", profile->total_bytes);
    fprintf(profile_file, "Chunk Size: %u bytes\n", profile->chunk_size);
    fprintf(profile_file, "Number of Chunks Sent: %u\n", profile->chunks);
    fprintf(profile_file, "Remaining Bytes: %d\n", profile->remainder);

    fprintf(profile_file, "\nConnection Time (microseconds): ");
    for (uint32_t i = 0; i < profile->count; ++i)
    {
        char print_format[] = "%llu, ";
        if (i == (profile->count - 1))
        {
            print_format[4] = '\0';
        }
        fprintf(profile_file, print_format,
                profile->data[i].end_time_connect -
                    profile->data[i].start_time_connect);
    }

    fprintf(profile_file,
            "\nFinding characteristic and service (microseconds): ");
    for (uint32_t i = 0; i < profile->count; ++i)
    {
        char print_format[] = "%llu, ";
        if (i == (profile->count - 1))
        {
            print_format[4] = '\0';
        }
        fprintf(profile_file, print_format,
                profile->data[i].end_time_find -
                    profile->data[i].start_time_find);
    }

    fprintf(profile_file, "\nTotal Sending Time (microseconds): ");
    for (uint32_t i = 0; i < profile->count; ++i)
    {
        char print_format[] = "%llu, ";
        if (i == (profile->count - 1))
        {
            print_format[4] = '\0';
        }
        fprintf(profile_file, print_format,
                profile->data[i].end_time_sending -
                    profile->data[i].start_time_sending);
    }

    fprintf(profile_file, "\n\nChunk Timing Details:\n");

    fprintf(profile_file, "Chunk round trip times (microseconds):\n");
    for (uint32_t i = 0; i < profile->data[0].time_chunk_before_count; i++)
    {
        fprintf(profile_file, "Chunk %u: ", i + 1);
        for (uint32_t j = 0; j < profile->count; ++j)
        {
            char print_format[] = "%llu, ";
            if (j == (profile->count - 1))
            {
                print_format[4] = '\n';
                print_format[5] = '\0';
            }
            fprintf(profile_file, print_format,
                    profile->data[j].time_buffer_chunk_sent[i] -
                        profile->data[j].time_buffer_chunk_before[i]);
        }
    }

    fprintf(profile_file, "\n----------------------------------------\n");

    fclose(profile_file);

    fopen_s(&profile_file, file_name_formatted, "a");

    fseek(profile_file, 0, SEEK_END);
    if (ftell(profile_file) == 0)
    {
        fprintf(profile_file,
                "Run,Buffer size,Total Sending Time (microseconds)\n");
    }

    for (uint32_t i = 0; i < profile->count; ++i)
    {
        uint64_t sending_time = profile->data[i].end_time_sending -
                                profile->data[i].start_time_sending;

        fprintf(profile_file, "%u,%u,%llu\n", run_offset + (i + 1), buffer_size,
                sending_time);
    }
    fclose(profile_file);

    fopen_s(&profile_file, file_name_mean, "a");

    fseek(profile_file, 0, SEEK_END);
    if (ftell(profile_file) == 0)
    {
        fprintf(profile_file, "Buffer_size,");
        for (uint32_t i = 0; i < profile->count; ++i)
        {
            fprintf(profile_file, "Time_run%u_microseconds", i);
            if (i < (profile->count - 1))
            {
                fprintf(profile_file, ",");
            }
        }
        fprintf(profile_file, "\n");
    }

    fprintf(profile_file, "%u,", buffer_size);
    for (uint32_t i = 0; i < profile->count; ++i)
    {

        uint64_t sending_time = profile->data[i].end_time_sending -
                                profile->data[i].start_time_sending;

        fprintf(profile_file, "%llu", sending_time);
        if (i < (profile->count - 1))
        {
            fprintf(profile_file, ",");
        }
    }
    fprintf(profile_file, "\n");

    fclose(profile_file);
}

#define TARGET_DEVICE_NAME L"WaveTablePP_2"

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

void connect_and_send_file(uint64_t device_address, const ByteArray& buffer,
                           Profile* profile, ProfileData* profile_data)
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

    profile_data->start_time_connect = get_time_micro();
    if (connect(bluetooth_socket, (SOCKADDR*)&sockaddr_bth,
                sizeof(sockaddr_bth)) == SOCKET_ERROR)
    {
        return;
    }
    profile_data->end_time_connect = get_time_micro();

    bool error = false;

    uint32_t chunk_size = buffer_size;
    uint32_t chunks = (uint32_t)buffer.size / chunk_size;
    uint32_t remainder = buffer.size % chunk_size;

    profile->total_bytes = buffer.size;
    profile->chunk_size = chunk_size;
    profile->chunks = chunks;
    profile->remainder = remainder;

#if 0
    printf(
        "\nStart sending data:\n\tTotal bytes to send: %u\n\tChunk size: %u\n\tTotal chunks: %u\n",
        buffer.size, chunk_size, chunks);
#else
    printf("\nStart sending data:\n");
#endif
    profile_data->start_time_sending = get_time_micro();

    uint32_t i = 0;
    for (; i < chunks && !error; i++)
    {
        // profile_data->time_buffer_chunk_before
        //    [profile_data->time_chunk_before_count++] = get_time_micro();

        int offset = i * chunk_size;
        if (send(bluetooth_socket, (char*)buffer.data + offset, chunk_size,
                 0) == SOCKET_ERROR)
        {
            return;
        }

        error = !wait_for_respons(bluetooth_socket, 0x08);

        // profile_data
        //    ->time_buffer_chunk_sent[profile_data->time_chunk_sent_count++] =
        //   get_time_micro();
    }
    uint32_t offset = i * chunk_size;
    if (!error && remainder > 0)
    {
        if (send(bluetooth_socket, (char*)buffer.data + offset, remainder, 0) ==
            SOCKET_ERROR)
        {
            return;
        }
    }
    error = !wait_for_respons(bluetooth_socket, 0x06);

    profile_data->end_time_sending = get_time_micro();

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

unsigned long find_device_address(void* address)
{
    BLUETOOTH_FIND_RADIO_PARAMS radio_params = {};
    radio_params.dwSize = sizeof(BLUETOOTH_FIND_RADIO_PARAMS);

    HANDLE radio_handle = NULL;
    HBLUETOOTH_RADIO_FIND radio_find =
        BluetoothFindFirstRadio(&radio_params, &radio_handle);

    if (!radio_find)
    {
        printf("Failed to find radio\n");
        return 1;
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
        return 1;
    }

    bool device_found = false;
    do
    {
        printf("Found device: %ls\n", device_info.szName);
        if (wcscmp(device_info.szName, TARGET_DEVICE_NAME) == 0)
        {
            printf("Found target device: %ls\n", device_info.szName);
            printf("Device address: %llu\n", device_info.Address.ullLong);
            device_found = true;

            break;
        }
    } while (BluetoothFindNextDevice(device_find, &device_info));

    BluetoothFindDeviceClose(device_find);
    BluetoothFindRadioClose(radio_find);

    (*(uint64_t*)address) = 0;
    if (device_found)
    {
        (*(uint64_t*)address) = device_info.Address.ullLong;
    }

    return 0;
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

void connect_and_send_file(uint64_t device_address, const ByteArray& buffer,
                           Profile* profile, ProfileData* profile_data)
{
    printf("Connecting\n");

    profile_data->start_time_connect = get_time_micro();

    auto device =
        BluetoothLEDevice::FromBluetoothAddressAsync(device_address).get();

    profile_data->end_time_connect = get_time_micro();

    profile_data->start_time_find = get_time_micro();

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

    profile_data->end_time_find = get_time_micro();

    // - 3 for GATT header, 512 per Bluetooth specification is max user values
    uint32_t mtu = min(512, target_service.Session().MaxPduSize() - 3);

    printf("Sending\n");
    uint32_t chunk_size = mtu;
    uint32_t chunks = buffer.size / chunk_size;
    uint32_t remainder = buffer.size % chunk_size;
    auto write_buffer = Windows::Storage::Streams::Buffer(chunk_size);

    profile->total_bytes = buffer.size;
    profile->chunk_size = mtu;
    profile->chunks = chunks;
    profile->remainder = remainder;

    profile_data->start_time_sending = get_time_micro();

    for (uint32_t i = 0; i < chunks; ++i)
    {
        uint32_t offset = chunk_size * i;

        //profile_data->time_buffer_chunk_before
         //   [profile_data->time_chunk_before_count++] = get_time_micro();

        write_buffer.Length(chunk_size);
        memcpy(write_buffer.data(), buffer.data + offset, chunk_size);

        auto writeResult =
            target_characteristic.WriteValueAsync(write_buffer).get();

        //profile_data
         //   ->time_buffer_chunk_sent[profile_data->time_chunk_sent_count++] =
          //  get_time_micro();
    }
    if (remainder > 0)
    {
        uint32_t offset = chunks * chunk_size;

        write_buffer.Length(remainder);
        memcpy(write_buffer.data(), buffer.data + offset, remainder);

        auto writeResult =
            target_characteristic.WriteValueAsync(write_buffer).get();
    }

    profile_data->end_time_sending = get_time_micro();

    printf("Done\n");

    device.Close();
}
#endif

int main(void)
{
    uint64_t device_address = 0;
#ifndef USE_SPP
    init_apartment();
    find_device_address(&device_address);
#else
    HANDLE thread_handle = CreateThread(0, 0, find_device_address, &device_address, 0, NULL);
#endif

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

    glfwSwapInterval(1);
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

    uint32_t run = 0;

    ByteArray converted_buffer = {};
    while (!glfwWindowShouldClose(window))
    {
        if (parse_file)
        {
            converted_buffer = process_audio_buffer(prompt.data, 64);
            parse_file = false;
            send_disable = false;
        }
        imgui_begin_frame();
        if (imgui_update(window))
        {
            if (device_address != 0)
            {
                uint32_t sleep_millis = 5 * 1000;
                uint32_t iterations = 10;
                Profile profile = { 0 };
                profile.data =
                    (ProfileData*)calloc(iterations, sizeof(ProfileData));
                uint32_t run_offset = run;
               // for (uint32_t i = 0; i < iterations; ++i)
                {
                    //printf("Test Iteration: [%u/%u]\n", (run++) + 1,
                     //      run_offset + iterations);
                    connect_and_send_file(device_address, converted_buffer,
                                          &profile, profile.data);
                   // if (i < (iterations - 1))
                    {
                     //   printf("Sleep for: %u seconds\n", sleep_millis / 1000);
                    //    Sleep(sleep_millis);
                    }
                }
                //profile.count = iterations;
                //printf("Test Done!\n");
                //profile_append_to_file_buffer(
                  //  &profile, "profile_data_spp_buffer_71kb.txt",
                   // "profile_data_spss_buffer_71kb.txt",
                    //"profile_data_spss_buffer_71kb_2.txt", run_offset);

                free(profile.data);
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
    WaitForSingleObject(thread_handle, INFINITE);
    CloseHandle(thread_handle);
#endif

    glfwTerminate();
}
