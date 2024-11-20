#include <winsock2.h>
#include <windows.h>
#include <bthsdpdef.h>
#include <bluetoothapis.h>
#include <ws2bth.h>

#include <stdio.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include <string>
#include <vector>
#include <algorithm>
#include <dr_wav/dr_wav.h>

#include <glad/glad.h>
#include <glfw/glfw3.h>

#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_opengl3.h>

#define TARGET_DEVICE_NAME "WaveTablePP"

typedef struct AudioBuffer
{
    uint32_t channels;
    uint32_t sample_rate;
    uint64_t size;
    float* data;
} AudioBuffer;

typedef struct ByteArray
{
    uint32_t size;
    uint8_t* data;
} ByteArray;

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

void connect_and_send_file(BTH_ADDR device_address, const ByteArray& buffer)
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
        if (send(bluetooth_socket, (char*)buffer.data + offset, chunk_size, 0) ==
            SOCKET_ERROR)
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

BTH_ADDR find_device_address(void)
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

std::string prompt = "Drop File";
bool parse_file = false;
bool send_disable = true;

void imgui_update(GLFWwindow* window, BTH_ADDR device_address, const ByteArray& buffer)
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
    ImVec2 text_size = ImGui::CalcTextSize(prompt.c_str());
    ImVec2 button_size = ImVec2(100.0f, 30.0f);

    ImVec2 text_pos =
        ImVec2((window_size.x - text_size.x) * 0.5f,
               (window_size.y - text_size.y - spacing - button_size.y) * 0.5f);

    ImGui::SetCursorPos(text_pos);

    ImGui::Text("%s", prompt.c_str());

    ImGui::PopFont();

    ImVec2 button_pos = ImVec2((window_size.x - button_size.x) * 0.5f,
                               text_pos.y + text_size.y + spacing);
    ImGui::SetCursorPos(button_pos);

    ImGui::BeginDisabled(send_disable);
    if (ImGui::Button("Send", button_size))
    {
        connect_and_send_file(device_address, buffer);
    }
    ImGui::EndDisabled();

    ImGui::End();

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void drop_callback(GLFWwindow* window, int count, const char** paths)
{
    prompt = paths[0];
    parse_file = true;
    for (int i = 0; i < count; i++)
    {
    }
}

void convert_stereo_to_mono_sum_average(AudioBuffer* audio_buffer)
{
    uint64_t half_size = audio_buffer->size / 2;
    float* converted_buffer = (float*)calloc(half_size, sizeof(float));

    for (uint64_t i = 0, j = 0; i < half_size; ++i, j += 2)
    {
        converted_buffer[i] =
            (audio_buffer->data[j] + audio_buffer->data[j + 1]) / 2.0f;
    }
    free(audio_buffer->data);
    audio_buffer->channels = 1;
    audio_buffer->size = half_size;
    audio_buffer->data = converted_buffer;
}

AudioBuffer get_sample_data(const char* file_name)
{
    AudioBuffer audio_buffer = {};
    uint64_t frame_count = 0;
    audio_buffer.data = drwav_open_file_and_read_pcm_frames_f32(
        file_name, &audio_buffer.channels, &audio_buffer.sample_rate,
        &frame_count, NULL);

    audio_buffer.size = frame_count * audio_buffer.channels;
    if (audio_buffer.channels == 2)
    {
        convert_stereo_to_mono_sum_average(&audio_buffer);
    }
    else if (audio_buffer.channels > 2)
    {
        // TODO: Log error
        free(audio_buffer.data);
        return audio_buffer;
    }

    return audio_buffer;
}

void average_magnitude_difference(const AudioBuffer& audio_buffer,
                                  std::vector<float>& shift_avg_difference)
{
    int maxShift = (int)audio_buffer.sample_rate / 20;
    for (int shift = 0; shift < maxShift; shift++)
    {
        float total_difference = 0;
        for (int j = shift; j < maxShift - shift; j++)
        {
            total_difference +=
                abs(audio_buffer.data[j] - audio_buffer.data[j + shift]);
        }
        float avg = total_difference / (audio_buffer.size - shift);
        if (avg > 0.001f)
        {
            shift_avg_difference.push_back(avg);
        }
    }
}

float* smooth_avg_difference(const std::vector<float>& shift_avg_difference,
                             int window_size)
{
    float* smoothed =
        (float*)calloc(shift_avg_difference.size(), sizeof(float));
    for (int i = 0; i < shift_avg_difference.size(); i++)
    {
        int start = max(0, i - (window_size / 2));
        int end =
            min((int)shift_avg_difference.size(), i + (window_size / 2) + 1);
        float sum = 0;
        for (int j = start; j < end; j++)
        {
            sum += shift_avg_difference[j];
        }
        smoothed[i] = sum / (end - start);
    }
    return smoothed;
}

void find_local_minima(std::vector<uint32_t>& local_minima, float* smooth,
                       int size)
{
    int scan_count = size / 10;
    for (int j = 0; j < size; ++j)
    {
        boolean jump = false;
        for (int h = 1; h <= scan_count; ++h)
        {
            int rightIndex = j + h;
            int leftIndex = j - h;
            if (leftIndex >= 0)
            {
                if (smooth[leftIndex] < smooth[j])
                {
                    jump = true;
                    break;
                }
            }
            if (rightIndex < size)
            {
                if (smooth[rightIndex] < smooth[j])
                {
                    jump = true;
                    break;
                }
            }
        }
        if (!jump)
        {
            local_minima.push_back((uint32_t)j);
        }
    }
}

uint16_t calculate_samples_per_cycle(const std::vector<uint32_t>& local_minima)
{
    std::vector<uint32_t> counts;
    for (int j = 1; j < local_minima.size(); ++j)
    {
        uint32_t sample_count = local_minima[j] - local_minima[j - 1];
        counts.push_back(sample_count);
    }

    std::sort(counts.begin(), counts.end());

    int median = 0;
    size_t size = counts.size();
    if (size % 2 == 0)
    {
        median = (counts[size / 2 - 1] + counts[size / 2]) / 2;
    }
    else
    {
        median = counts[size / 2];
    }

    int threshold = 8;
    std::vector<uint32_t> filtered_counts;
    for (int count : counts)
    {
        if (abs(count - median) <= threshold)
        {
            filtered_counts.push_back(count);
        }
    }

    uint32_t sum = 0;
    for (uint32_t filtered_count : filtered_counts)
    {
        sum += filtered_count;
    }
    return (uint16_t)(sum / filtered_counts.size());
}

uint8_t low(uint16_t value)
{
    return (uint8_t)(value & 0xFF);
}

uint8_t high(uint16_t value)
{
    return (uint8_t)((value >> 8) & 0xFF);
}

ByteArray process_audio_buffer(const char* file_name)
{
    AudioBuffer audio_buffer = get_sample_data(file_name);

    std::vector<float> shift_avg_difference;
    average_magnitude_difference(audio_buffer, shift_avg_difference);
    float* smoothed = smooth_avg_difference(shift_avg_difference, 5);
    std::vector<uint32_t> local_minima;
    find_local_minima(local_minima, smoothed, (int)shift_avg_difference.size());
    free(smoothed);
    uint16_t samples_per_cycle = calculate_samples_per_cycle(local_minima);

    uint32_t header_size = 14;
    uint32_t total_bytes = ((uint32_t)audio_buffer.size * 2) + header_size;

    uint16_t id0 = 29960;
    uint16_t id1 = 62903;
    uint16_t id2 = 35185;
    uint16_t id3 = 26662;

    uint8_t samples_per_cycle_low = low(samples_per_cycle);
    uint8_t samples_per_cycle_high = high(samples_per_cycle);

    uint8_t total_bytes_high0 = (uint8_t)((total_bytes >> 24) & 0xFF);
    uint8_t total_bytes_high1 = (uint8_t)((total_bytes >> 16) & 0xFF);
    uint8_t total_bytes_low0 = (uint8_t)((total_bytes >> 8) & 0xFF);
    uint8_t total_bytes_low1 = (uint8_t)((total_bytes) & 0xFF);

    uint8_t header[] = {
        high(id0),
        low(id0),
        high(id1),
        low(id1),
        high(id2),
        low(id2),
        high(id3),
        low(id3),
        samples_per_cycle_high,
        samples_per_cycle_low,
        total_bytes_high0,
        total_bytes_high1,
        total_bytes_low0,
        total_bytes_low1,
    };

    ByteArray result = {};
    result.size = total_bytes;
    result.data = (uint8_t*)calloc(total_bytes, sizeof(uint8_t));

    for (uint32_t i = 0; i < header_size; ++i)
    {
        result.data[i] = header[i];
    }

    for (uint64_t i = 0, j = header_size; i < audio_buffer.size; ++i)
    {
        uint16_t converted_value =
            (uint16_t)((audio_buffer.data[i] + 1.0f) * 32767.5f);
        result.data[j++] = high(converted_value);
        result.data[j++] = low(converted_value);
    }
    return result;
}

int main(void)
{
    BTH_ADDR device_address = find_device_address();

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

    if (device_address != 0)
    {
        WSADATA wsa_data;
        if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0)
        {
            return 1;
        }

        ByteArray converted_buffer = {};
        while (!glfwWindowShouldClose(window))
        {
            if (parse_file)
            {
                converted_buffer =
                    process_audio_buffer(prompt.c_str());
                parse_file = false;
                send_disable = false;
            }
            imgui_begin_frame();
            imgui_update(window, device_address, converted_buffer);
            glfwSwapBuffers(window);
            glfwPollEvents();
        }

        WSACleanup();
    }

    glfwTerminate();
}
