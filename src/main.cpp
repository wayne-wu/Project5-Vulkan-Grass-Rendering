#include <vulkan/vulkan.h>
#include <sstream>
#include "Instance.h"
#include "Window.h"
#include "Renderer.h"
#include "Camera.h"
#include "Scene.h"
#include "Image.h"

Device* device;
SwapChain* swapChain;
Renderer* renderer;
Camera* camera;

namespace {
    void resizeCallback(GLFWwindow* window, int width, int height) {
        if (width == 0 || height == 0) return;

        vkDeviceWaitIdle(device->GetVkDevice());
        swapChain->Recreate();
        renderer->RecreateFrameResources();
    }

    bool leftMouseDown = false;
    bool rightMouseDown = false;
    double previousX = 0.0;
    double previousY = 0.0;

    void mouseDownCallback(GLFWwindow* window, int button, int action, int mods) {
        if (button == GLFW_MOUSE_BUTTON_LEFT) {
            if (action == GLFW_PRESS) {
                leftMouseDown = true;
                glfwGetCursorPos(window, &previousX, &previousY);
            }
            else if (action == GLFW_RELEASE) {
                leftMouseDown = false;
            }
        } else if (button == GLFW_MOUSE_BUTTON_RIGHT) {
            if (action == GLFW_PRESS) {
                rightMouseDown = true;
                glfwGetCursorPos(window, &previousX, &previousY);
            }
            else if (action == GLFW_RELEASE) {
                rightMouseDown = false;
            }
        }
    }

    void mouseMoveCallback(GLFWwindow* window, double xPosition, double yPosition) {
        if (leftMouseDown) {
            double sensitivity = 0.5;
            float deltaX = static_cast<float>((previousX - xPosition) * sensitivity);
            float deltaY = static_cast<float>((previousY - yPosition) * sensitivity);

            camera->UpdateOrbit(deltaX, deltaY, 0.0f);

            previousX = xPosition;
            previousY = yPosition;
        } else if (rightMouseDown) {
            double deltaZ = static_cast<float>((previousY - yPosition) * 0.05);

            camera->UpdateOrbit(0.0f, 0.0f, deltaZ);

            previousY = yPosition;
        }
    }
}

int main() {
    static constexpr char* applicationName = "Vulkan Grass Rendering";
    InitializeWindow(1280, 960, applicationName);

    unsigned int glfwExtensionCount = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    
    //NOTE: First thing is to create an instance -> connection between application and the Vulkan library
    Instance* instance = new Instance(applicationName, glfwExtensionCount, glfwExtensions);

    //NOTE: Connection between Vulkan and the windows system to present result to the screen
    VkSurfaceKHR surface;
    if (glfwCreateWindowSurface(instance->GetVkInstance(), GetGLFWWindow(), nullptr, &surface) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create window surface");
    }

    //NOTE: Need to select a physical GPU device to use
    instance->PickPhysicalDevice({ VK_KHR_SWAPCHAIN_EXTENSION_NAME, VK_EXT_SAMPLER_FILTER_MINMAX_EXTENSION_NAME }, QueueFlagBit::GraphicsBit | QueueFlagBit::TransferBit | QueueFlagBit::ComputeBit | QueueFlagBit::PresentBit, surface);

    VkPhysicalDeviceFeatures deviceFeatures = {};
    deviceFeatures.tessellationShader = VK_TRUE;
    deviceFeatures.fillModeNonSolid = VK_TRUE;
    deviceFeatures.samplerAnisotropy = VK_TRUE;

    //NOTE: Create the logical device here and connecting to the physical device
    device = instance->CreateDevice(QueueFlagBit::GraphicsBit | QueueFlagBit::TransferBit | QueueFlagBit::ComputeBit | QueueFlagBit::PresentBit, deviceFeatures);

    //NOTE: Swap chain is essentially a queue of images that are waiting to be presented to the screen
    swapChain = device->CreateSwapChain(surface, 5);

    camera = new Camera(device, 640.f / 480.f);

    VkCommandPoolCreateInfo transferPoolInfo = {};
    transferPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    transferPoolInfo.queueFamilyIndex = device->GetInstance()->GetQueueFamilyIndices()[QueueFlags::Transfer];
    transferPoolInfo.flags = 0;

    VkCommandPool transferCommandPool;
    if (vkCreateCommandPool(device->GetVkDevice(), &transferPoolInfo, nullptr, &transferCommandPool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create command pool");
    }

    VkImage grassImage;
    VkDeviceMemory grassImageMemory;
    Image::FromFile(device,
        transferCommandPool,
        "images/grass.jpg",
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        grassImage,
        grassImageMemory
    );

    float planeDim = 15.f;
    float halfWidth = planeDim * 0.5f;
    Model* plane = new Model(device, transferCommandPool,
        {
            { { -halfWidth, 0.0f, halfWidth }, { 1.0f, 0.0f, 0.0f },{ 1.0f, 0.0f } },
            { { halfWidth, 0.0f, halfWidth }, { 0.0f, 1.0f, 0.0f },{ 0.0f, 0.0f } },
            { { halfWidth, 0.0f, -halfWidth }, { 0.0f, 0.0f, 1.0f },{ 0.0f, 1.0f } },
            { { -halfWidth, 0.0f, -halfWidth }, { 1.0f, 1.0f, 1.0f },{ 1.0f, 1.0f } }
        },
        { 0, 1, 2, 2, 3, 0 }
    );
    plane->SetTexture(grassImage);

    VkImage brickImage;
    VkDeviceMemory brickImageMemory;
    Image::FromFile(device,
      transferCommandPool,
      "images/brick.jpg",
      VK_FORMAT_R8G8B8A8_UNORM,
      VK_IMAGE_TILING_OPTIMAL,
      VK_IMAGE_USAGE_SAMPLED_BIT,
      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
      brickImage,
      brickImageMemory
    );

    halfWidth = 3.f;
    Model* cube = new Model(device, transferCommandPool,
        {
            // front
            { { -halfWidth, -halfWidth, halfWidth }, { 1.0f, 0.0f, 0.0f },{ 1.0f, 0.0f } },
            { { halfWidth, -halfWidth, halfWidth }, { 0.0f, 1.0f, 0.0f },{ 0.0f, 0.0f } },
            { { halfWidth, halfWidth, halfWidth }, { 0.0f, 0.0f, 1.0f },{ 0.0f, 1.0f } },
            { { -halfWidth, halfWidth, halfWidth }, { 1.0f, 1.0f, 1.0f },{ 1.0f, 1.0f } },
            // back
            { { -halfWidth, -halfWidth, -halfWidth }, { 1.0f, 0.0f, 0.0f },{ 1.0f, 1.0f } },
            { { halfWidth, -halfWidth, -halfWidth }, { 0.0f, 1.0f, 0.0f },{ 0.0f, 1.0f } },
            { { halfWidth, halfWidth, -halfWidth }, { 0.0f, 0.0f, 1.0f },{ 0.0f, 0.0f } },
            { { -halfWidth, halfWidth, -halfWidth }, { 1.0f, 1.0f, 1.0f },{ 1.0f, 0.0f } }
        },
        { 0, 1, 2, 2, 3, 0, 1, 5, 6, 6, 2, 1, 7, 6, 5, 5, 4, 7, 4, 0, 3, 3, 7, 4, 4, 5, 1, 1, 0, 4, 3, 2, 6, 6, 7, 3 }
    );
    cube->SetTexture(brickImage);
    
    // NOTE: Blades class contains ALL blades (i.e. vector is contained within the class)
    Blades* blades = new Blades(device, transferCommandPool, planeDim);

    vkDestroyCommandPool(device->GetVkDevice(), transferCommandPool, nullptr);

    Scene* scene = new Scene(device);

    scene->AddModel(plane);
    scene->AddModel(cube);
    scene->AddBlades(blades);

    renderer = new Renderer(device, swapChain, scene, camera);
    
    GLFWwindow* window = GetGLFWWindow();
    glfwSetWindowSizeCallback(window, resizeCallback);
    glfwSetMouseButtonCallback(window, mouseDownCallback);
    glfwSetCursorPosCallback(window, mouseMoveCallback);

    double fps = 0;
    double timebase = 0;
    int frame = 0;

    while (!ShouldQuit()) {

        glfwPollEvents();

        frame++;
        double time = glfwGetTime();

        if (time - timebase > 1.0) {
          fps = frame / (time - timebase);
          timebase = time;
          frame = 0;
        }

        scene->UpdateTime();
        renderer->Frame();

        std::ostringstream ss;
        ss << "[";
        ss.precision(1);
        ss << std::fixed << fps;
        ss << " fps] " << applicationName;
        glfwSetWindowTitle(window, ss.str().c_str());
    }

    vkDeviceWaitIdle(device->GetVkDevice());

    vkDestroyImage(device->GetVkDevice(), grassImage, nullptr);
    vkFreeMemory(device->GetVkDevice(), grassImageMemory, nullptr);
    vkDestroyImage(device->GetVkDevice(), brickImage, nullptr);
    vkFreeMemory(device->GetVkDevice(), brickImageMemory, nullptr);

    delete scene;
    delete plane;
    delete cube;
    delete blades;
    delete camera;
    delete renderer;
    delete swapChain;
    delete device;
    delete instance;
    DestroyWindow();
    return 0;
}
