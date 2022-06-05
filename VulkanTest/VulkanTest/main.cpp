#define NOMINMAX // for fixing `std::max - expected an identifier` error
#define VK_USE_PLATFORM_WIN32_KHR
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#include <cstdint> // Necessary for uint32_t
#include <limits> // Necessary for std::numeric_limits
#include <algorithm> // Necessary for std::clamp
#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <vector>
#include <optional>
#include <set>


// the resolution {WIDTH, HEIGHT} that we specified earlier when creating the window is measured in screen coordinates
const uint32_t WIDTH = 800;
const uint32_t HEIGHT = 600;

// validation layers are used for:
//   Checking the values of parameters against the specification to detect misuse
//   Tracking creationand destruction of objects to find resource leaks
//   Checking thread safety by tracking the threads that calls originate from
//   Logging every calland its parameters to the standard output
//   Tracing Vulkan calls for profilingand replaying
const std::vector<const char*> validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

// Vulkan does not have the concept of a "default framebuffer", 
// hence it requires an infrastructure that will own the buffers we will render to before we visualize them on the screen. 
// This infrastructure is known as the swap chain and must be created explicitly in Vulkan. 
// The swap chain is essentially a queue of images that are waiting to be presented to the screen
// You have to enable the VK_KHR_swapchain device extension after querying for its support.
const std::vector<const char*> deviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif


struct QueueFamilyIndices {
    // `std::optional` is a wrapper that contains no value until you assign something to it
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;

    bool isComplete() {
        return graphicsFamily.has_value() && presentFamily.has_value();
    }
};

// There are basically three kinds of properties we need to check in swap chain:
//   Basic surface capabilities(min / max number of images in swap chain, min / max width and height of images)
//   Surface formats(pixel format, color space)
//   Available presentation modes
struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};


class HelloTriangleApplication {
public:
    void run() {
        initWindow();
        initVulkan();
        mainLoop();
        cleanup();
    }

private:
    GLFWwindow* window;
    VkInstance instance;
    VkDebugUtilsMessengerEXT debugMessenger;
    VkPhysicalDevice physicalDevice;
    VkDevice device;
    VkQueue graphicsQueue;
    VkSurfaceKHR surface;
    VkQueue presentQueue;
    VkSwapchainKHR swapChain;
    std::vector<VkImage> swapChainImages;
    VkFormat swapChainImageFormat;
    VkExtent2D swapChainExtent;
    std::vector<VkImageView> swapChainImageViews;


    void initWindow() {
        std::cout << ">>> initWindow" << std::endl;

        // initializes the GLFW library
        glfwInit();

        // Because GLFW was originally designed to create an OpenGL context, we need to tell it to not create an OpenGL context
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

        // disable window resizing
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

        // The first three parameters specify the width, height and title of the window.
        // The fourth parameter allows you to optionally specify a monitor to open the window on 
        // and the last parameter is only relevant to OpenGL.
        window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);

        std::cout << "<<< initWindow" << std::endl;
    }

    // checks if all of the requested layers are available
    bool checkValidationLayerSupport() {
        uint32_t layerCount;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

        std::vector<VkLayerProperties> availableLayers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

        for (const char* layerName : validationLayers) {
            bool layerFound = false;

            for (const auto& layerProperties : availableLayers) {
                if (strcmp(layerName, layerProperties.layerName) == 0) {
                    std::cout << "Found validation layer: " << layerName << std::endl;
                    layerFound = true;
                    break;
                }
            }

            if (!layerFound) {
                return false;
            }
        }

        return true;
    }

    // return the required list of extensions based on whether validation layers are enabled or not
    std::vector<const char*> getRequiredExtensions() {
        uint32_t glfwExtensionCount = 0;
        const char** glfwExtensions;
        glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

        std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

        if (enableValidationLayers) {
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }

        return extensions;
    }

    void createInstance() {
        std::cout << ">>> createInstance" << std::endl;

        if (enableValidationLayers && !checkValidationLayerSupport()) {
            throw std::runtime_error("validation layers requested, but not available!");
        }

        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "Hello Triangle";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "No Engine";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_0;

        VkInstanceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;

        // global validation layers to enable
        auto extensions = getRequiredExtensions();
        createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        createInfo.ppEnabledExtensionNames = extensions.data();
        VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
        if (enableValidationLayers) {
            createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
            createInfo.ppEnabledLayerNames = validationLayers.data();

            populateDebugMessengerCreateInfo(debugCreateInfo);
            createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
        }
        else {
            createInfo.enabledLayerCount = 0;

            createInfo.pNext = nullptr;
        }

        // 1st arg: Pointer to struct with creation info
        // 2nd arg: Pointer to custom allocator callbacks
        // 3rd arg: Pointer to the variable that stores the handle to the new object
        if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
            throw std::runtime_error("failed to create instance!");
        }

        // First get the number of extensions
        uint32_t extensionCount = 0;
        vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
        // Then get extension details
        std::vector<VkExtensionProperties> extensionsAll(extensionCount);
        vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensionsAll.data());

        // Each `VkExtensionProperties` struct contains the name and version of an extension
        std::cout << "available extensions:\n";
        for (const auto& extension : extensionsAll) {
            std::cout << "\t" << extension.extensionName << " (version: " << extension.specVersion << ")" << std::endl;
        }

        std::cout << "<<< createInstance" << std::endl;
    }

    // a debug callback function
    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData) {

        // The `messageSeverity` parameter specifies the severity of the message, which is one of the following flags:
        //   VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT: Diagnostic message
        //   VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT : Informational message like the creation of a resource
        //   VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT : Message about behavior that is not necessarily an error, but very likely a bug in your application
        //   VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT : Message about behavior that is invalidand may cause crashes
        if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
            // Message is important enough to show
        }

        // The `messageType` parameter can have the following values:
        //   VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT: Some event has happened that is unrelated to the specification or performance
        //   VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT : Something has happened that violates the specification or indicates a possible mistake
        //   VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT : Potential non-optimal use of Vulkan

        // The `pCallbackData` parameter refers to a VkDebugUtilsMessengerCallbackDataEXT struct containing the details of the message itself, with the most important members being:
        //   pMessage: The debug message as a null-terminated string
        //   pObjects : Array of Vulkan object handles related to the message
        //   objectCount : Number of objects in array

        // The `pUserData` parameter contains a pointer that was specified during the setup of the callback and allows you to pass your own data to it

        std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;

        // If the callback returns true, then the call is aborted with the VK_ERROR_VALIDATION_FAILED_EXT error
        return VK_FALSE;
    }

    void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo) {
        createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        createInfo.pfnUserCallback = debugCallback;
        createInfo.pUserData = nullptr; // Optional
    }

    // Find `vkCreateDebugUtilsMessengerEXT` function's address
    VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger) {
        auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
        if (func != nullptr) {
            return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
        }
        else {
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }
    }

    void setupDebugMessenger() {
        std::cout << ">>> setupDebugMessenger" << std::endl;

        if (!enableValidationLayers) return;

        VkDebugUtilsMessengerCreateInfoEXT createInfo;
        populateDebugMessengerCreateInfo(createInfo);

        // This struct should be passed to the `vkCreateDebugUtilsMessengerEXT` function to create the `VkDebugUtilsMessengerEXT` object. 
        // Unfortunately, because this function is an extension function, it is not automatically loaded. 
        // We have to look up its address ourselves using `vkGetInstanceProcAddr`
        if (CreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &debugMessenger) != VK_SUCCESS) {
            throw std::runtime_error("failed to set up debug messenger!");
        }

        std::cout << "<<< setupDebugMessenger" << std::endl;
    }

    // To establish the connection between Vulkan and the window system to present results to the screen, we need to use the WSI (Window System Integration) extensions
    // The `VK_KHR_surface` extension is an instance level extension, it needs the HWND and HMODULE handles on Windows,
    // which on Windows is called `VK_KHR_win32_surface` and is also automatically included in the list from `glfwGetRequiredInstanceExtensions`
    void createSurface() {
        std::cout << ">>> createSurface" << std::endl;

        // `glfwCreateWindowSurface` function does the following job:
        //VkWin32SurfaceCreateInfoKHR createInfo{};
        //createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
        //createInfo.hwnd = glfwGetWin32Window(window);
        //createInfo.hinstance = GetModuleHandle(nullptr);
        //vkCreateWin32SurfaceKHR(instance, &createInfo, nullptr, &surface)

        if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS) {
            throw std::runtime_error("failed to create window surface!");
        }

        std::cout << "<<< createSurface" << std::endl;
    }

    // almost every operation in Vulkan, anything from drawing to uploading textures, requires commands to be submitted to a queue
    // There are different types of queues that originate from different queue families and each family of queues allows only a subset of commands
    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device) {
        QueueFamilyIndices indices;

        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
        // The `VkQueueFamilyProperties` struct contains some details about the queue family, 
        // including the type of operations that are supported and the number of queues that can be created based on that family
        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

        std::cout << "Found " << queueFamilies.size() << " queue families" << std::endl;

        int i = 0;
        for (const auto& queueFamily : queueFamilies) {
            // ensure that a device can present images to the surface we created
            // the presentation is a queue-specific feature
            if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                std::cout << "Found graphics support in queue family: " << i << std::endl;
                indices.graphicsFamily = i;
            }
            VkBool32 presentSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
            if (presentSupport) {
                std::cout << "Found surface support in queue family: " << i << std::endl;
                indices.presentFamily = i;
            }
            if (indices.isComplete()) {
                break;
            }

            i++;
        }

        return indices;
    }

    bool checkDeviceExtensionSupport(VkPhysicalDevice device) {
        uint32_t extensionCount;
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

        std::vector<VkExtensionProperties> availableExtensions(extensionCount);
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

        std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

        for (const auto& extension : availableExtensions) {
            std::cout << "Checking swap chain extension: " << extension.extensionName << std::endl;
            requiredExtensions.erase(extension.extensionName);
        }

        return requiredExtensions.empty();
    }

    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device) {
        // the basic surface capabilities
        SwapChainSupportDetails details;
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

        // query the supported surface formats
        uint32_t formatCount;
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);
        if (formatCount != 0) {
            details.formats.resize(formatCount);
            vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
        }
        std::cout << "Found " << formatCount << " supported surface formats" << std::endl;

        // query the supported presentation modes
        uint32_t presentModeCount;
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);
        if (presentModeCount != 0) {
            details.presentModes.resize(presentModeCount);
            vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
        }
        std::cout << "Found " << presentModeCount << " supported present modes" << std::endl;

        return details;
    }

    // check if any of the physical devices meet the requirements
    bool isDeviceSuitable(VkPhysicalDevice device) {
        bool foundDevice = false;
        bool foundQueueFamily = false;

        // Basic device properties like the name, typeand supported Vulkan version
        VkPhysicalDeviceProperties deviceProperties;
        vkGetPhysicalDeviceProperties(device, &deviceProperties);

        // optional features like texture compression, 64 bit floats and multi viewport rendering (useful for VR)
        VkPhysicalDeviceFeatures deviceFeatures;
        vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

        if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU &&
            deviceFeatures.geometryShader) {
            std::cout << "Found physical device: " << deviceProperties.deviceName << std::endl;
            foundDevice = true;
        }

        // Find supported queue family on device
        QueueFamilyIndices indices = findQueueFamilies(device);
        if (indices.isComplete()) {
            std::cout << "Found available queue family" << std::endl;
            foundQueueFamily = true;
        }

        // Find swap chain extension on device
        bool extensionsSupported = checkDeviceExtensionSupport(device);

        // Check requirements of swap chain
        bool swapChainAdequate = false;
        if (extensionsSupported) {
            std::cout << "Found available swap chain" << std::endl;
            SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device);
            swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
            if (swapChainAdequate) {
                std::cout << "Found supported swap chain" << std::endl;
            }
        }

        return foundDevice && foundQueueFamily && swapChainAdequate;
    }

    void pickPhysicalDevice() {
        std::cout << ">>> pickPhysicalDevice" << std::endl;

        physicalDevice = VK_NULL_HANDLE;

        // Listing the graphics cards
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
        if (deviceCount == 0) {
            throw std::runtime_error("failed to find GPUs with Vulkan support!");
        }
        // an array to hold all of the `VkPhysicalDevice` handles
        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

        for (const auto& device : devices) {
            if (isDeviceSuitable(device)) {
                physicalDevice = device;
                break;
            }
        }

        if (physicalDevice == VK_NULL_HANDLE) {
            throw std::runtime_error("failed to find a suitable GPU!");
        }

        std::cout << "<<< pickPhysicalDevice" << std::endl;
    }

    void createLogicalDevice() {
        std::cout << ">>> createLogicalDevice" << std::endl;

        QueueFamilyIndices indices = findQueueFamilies(physicalDevice);

        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
        std::set<uint32_t> uniqueQueueFamilies = { indices.graphicsFamily.value(), indices.presentFamily.value() };

        float queuePriority = 1.0f;
        for (uint32_t queueFamily : uniqueQueueFamilies) {
            VkDeviceQueueCreateInfo queueCreateInfo{};
            queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCreateInfo.queueFamilyIndex = queueFamily;
            queueCreateInfo.queueCount = 1;
            // Vulkan lets you assign priorities to queues to influence the scheduling of command buffer execution using floating point numbers between 0.0 and 1.0. 
            // This is required even if there is only a single queue
            queueCreateInfo.pQueuePriorities = &queuePriority;
            queueCreateInfos.push_back(queueCreateInfo);
        }

        // the set of device features that we'll be using
        // An example of a device specific extension is `VK_KHR_swapchain`, which allows you to present rendered images from that device to windows
        VkPhysicalDeviceFeatures deviceFeatures{};

        VkDeviceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
        createInfo.pQueueCreateInfos = queueCreateInfos.data();
        createInfo.pEnabledFeatures = &deviceFeatures;
        // Enable device extensions (such as swap chain)
        createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
        createInfo.ppEnabledExtensionNames = deviceExtensions.data();
        // Previous implementations of Vulkan made a distinction between instance and device specific validation layers, but this is no longer the case.
        // That means that the `enabledLayerCount` and `ppEnabledLayerNames` fields of `VkDeviceCreateInfo` are ignored by up-to-date implementations
        if (enableValidationLayers) {
            createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
            createInfo.ppEnabledLayerNames = validationLayers.data();
        }
        else {
            createInfo.enabledLayerCount = 0;
        }

        if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS) {
            throw std::runtime_error("failed to create logical device!");
        }

        // The queues are automatically created along with the logical device
        // Device queues are implicitly cleaned up when the device is destroyed
        vkGetDeviceQueue(device, indices.graphicsFamily.value(), 0, &graphicsQueue);
        vkGetDeviceQueue(device, indices.presentFamily.value(), 0, &presentQueue);

        std::cout << "<<< createLogicalDevice" << std::endl;
    }

    // Surface format (color depth)
    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
        // Each VkSurfaceFormatKHR entry contains a format and a colorSpace member
        // Prefer SRGB color space
        for (const auto& availableFormat : availableFormats) {
            if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                std::cout << "Found preferred surface format" << std::endl;
                return availableFormat;
            }
        }
        return availableFormats[0];
    }

    // The presentation mode represents the actual conditions for showing images to the screen. 
    // There are four possible modes available in Vulkan:
    //   VK_PRESENT_MODE_IMMEDIATE_KHR: Images submitted by your application are transferred to the screen right away, which may result in tearing.
    //   VK_PRESENT_MODE_FIFO_KHR : The swap chain is a queue where the display takes an image from the front of the queue when the display is refreshed
    //     and the program inserts rendered images at the back of the queue.If the queue is full then the program has to wait.
    //     This is most similar to vertical sync as found in modern games.The moment that the display is refreshed is known as "vertical blank".
    //   VK_PRESENT_MODE_FIFO_RELAXED_KHR : This mode only differs from the previous one if the application is lateand the queue was empty at the last vertical blank.
    //     Instead of waiting for the next vertical blank, the image is transferred right away when it finally arrives.This may result in visible tearing.
    //   VK_PRESENT_MODE_MAILBOX_KHR : This is another variation of the second mode.Instead of blocking the application when the queue is full, 
    //     the images that are already queued are simply replaced with the newer ones.
    //     This mode can be used to render frames as fast as possible while still avoiding tearing, resulting in fewer latency issues than standard vertical sync.
    //     This is commonly known as "triple buffering", although the existence of three buffers alone does not necessarily mean that the framerate is unlocked.
    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) {
        // On PC platform, prefer VK_PRESENT_MODE_MAILBOX_KHR
        // On mobile platform, prefer VK_PRESENT_MODE_FIFO_KHR, due to energy usage
        for (const auto& availablePresentMode : availablePresentModes) {
            if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
                std::cout << "Found preferred present mode" << std::endl;
                return availablePresentMode;
            }
        }
        return VK_PRESENT_MODE_FIFO_KHR;
    }

    // The swap extent is the resolution of the swap chain images 
    // and it's almost always exactly equal to the resolution of the window that we're drawing to in pixels
    // On a high DPI display (like Apple's Retina display), screen coordinates don't correspond to pixels. 
    // Instead, due to the higher pixel density, the resolution of the window in pixel will be larger than the resolution in screen coordinates
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) {
        if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
            return capabilities.currentExtent;
        }
        else {
            int width, height;
            glfwGetFramebufferSize(window, &width, &height);

            VkExtent2D actualExtent = {
                static_cast<uint32_t>(width),
                static_cast<uint32_t>(height)
            };

            actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
            actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
            
            std::cout << "Size of swap extent: (" << actualExtent.width << ", " << actualExtent.height << ")" << std::endl;
            return actualExtent;
        }
    }

    void createSwapChain() {
        std::cout << ">>> createSwapChain" << std::endl;

        SwapChainSupportDetails swapChainSupport = querySwapChainSupport(physicalDevice);

        VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
        VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
        VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

        // simply sticking to this minimum means that we may sometimes have to wait on the driver to complete internal operations before we can acquire another image to render
        uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
        // 0 is a special value that means that there is no maximum
        if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
            imageCount = swapChainSupport.capabilities.maxImageCount;
        }
        std::cout << "Swap chain image count: " << imageCount << std::endl;

        // specify which surface the swap chain should be tied to
        VkSwapchainCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = surface;
        createInfo.minImageCount = imageCount;
        createInfo.imageFormat = surfaceFormat.format;
        createInfo.imageColorSpace = surfaceFormat.colorSpace;
        createInfo.imageExtent = extent;
        // The `imageArrayLayers` specifies the amount of layers each image consists of.This is always 1 unless you are developing a stereoscopic 3D application
        createInfo.imageArrayLayers = 1;
        // The `imageUsage` bit field specifies what kind of operations we'll use the images in the swap chain for
        //   `VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT` means rendering directly, which means that they're used as color attachment
        //   `VK_IMAGE_USAGE_TRANSFER_DST_BIT` means rendering images to a separate image first to perform operations like post-processing
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
        uint32_t queueFamilyIndices[] = { indices.graphicsFamily.value(), indices.presentFamily.value() };

        // There are two ways to handle images that are accessed from multiple queues:
        //   VK_SHARING_MODE_EXCLUSIVE: An image is owned by one queue family at a timeand ownership must be explicitly transferred before using it in another queue family.
        //     This option offers the best performance.
        //   VK_SHARING_MODE_CONCURRENT : Images can be used across multiple queue families without explicit ownership transfers.
        // If the queue families differ, then we'll be using the concurrent mode in this tutorial to avoid having to do the ownership chapters
        // If the graphics queue family and presentation queue family are the same, which will be the case on most hardware, then we should stick to exclusive mode
        if (indices.graphicsFamily != indices.presentFamily) {
            std::cout << "Swap chain image sharing mode: VK_SHARING_MODE_CONCURRENT" << std::endl;
            createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            createInfo.queueFamilyIndexCount = 2;
            createInfo.pQueueFamilyIndices = queueFamilyIndices;
        }
        else {
            std::cout << "Swap chain image sharing mode: VK_SHARING_MODE_EXCLUSIVE" << std::endl;
            createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
            createInfo.queueFamilyIndexCount = 0; // Optional
            createInfo.pQueueFamilyIndices = nullptr; // Optional
        }

        // a certain transform can be applied to images in the swap chain, like a 90 degree clockwise rotation or horizontal flip
        createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
        // The `compositeAlpha` field specifies if the alpha channel should be used for blending with other windows in the window system
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

        createInfo.presentMode = presentMode;
        // means that we don't care about the color of pixels that are obscured, for example because another window is in front of them
        // enabling for better performance
        createInfo.clipped = VK_TRUE;
        // it's possible that your swap chain becomes invalid or unoptimized while your application is running, for example because the window was resized. 
        // In that case the swap chain actually needs to be recreated from scratch and a reference to the old one must be specified in this field
        createInfo.oldSwapchain = VK_NULL_HANDLE;

        if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapChain) != VK_SUCCESS) {
            throw std::runtime_error("failed to create swap chain!");
        }

        // Create a set of images that can be drawn onto and can be presented to the window
        vkGetSwapchainImagesKHR(device, swapChain, &imageCount, nullptr);
        swapChainImages.resize(imageCount);
        vkGetSwapchainImagesKHR(device, swapChain, &imageCount, swapChainImages.data());

        swapChainImageFormat = surfaceFormat.format;
        swapChainExtent = extent;

        std::cout << "<<< createSwapChain" << std::endl;
    }

    // An image view is quite literally a view into an image. It describes how to access the image and which part of the image to access, 
    // for example if it should be treated as a 2D texture depth texture without any mipmapping levels.
    void createImageViews() {
        std::cout << ">>> createImageViews" << std::endl;

        swapChainImageViews.resize(swapChainImages.size());

        for (size_t i = 0; i < swapChainImages.size(); i++) {
            VkImageViewCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            createInfo.image = swapChainImages[i];
            // The `viewType` parameter allows you to treat images as 1D textures, 2D textures, 3D textures and cube maps.
            createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            createInfo.format = swapChainImageFormat;
            // The `components` field allows you to swizzle the color channels around
            createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
            // The `subresourceRange` field describes what the image's purpose is and which part of the image should be accessed
            // If you were working on a stereographic 3D application, then you would create a swap chain with multiple layers. 
            // You could then create multiple image views for each image representing the views for the left and right eyes by accessing different layers.
            createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            createInfo.subresourceRange.baseMipLevel = 0;
            createInfo.subresourceRange.levelCount = 1;
            createInfo.subresourceRange.baseArrayLayer = 0;
            createInfo.subresourceRange.layerCount = 1;

            if (vkCreateImageView(device, &createInfo, nullptr, &swapChainImageViews[i]) != VK_SUCCESS) {
                throw std::runtime_error("failed to create image views!");
            }
        }

        std::cout << "<<< createImageViews" << std::endl;
    }

    void initVulkan() {
        std::cout << ">>> initVulkan" << std::endl;

        createInstance();

        // The `vkCreateDebugUtilsMessengerEXT` call requires a valid instance to have been created
        setupDebugMessenger();

        // The window surface needs to be created right after the instance creation, because it can actually influence the physical device selection
        createSurface();

        pickPhysicalDevice();

        // You can even create multiple logical devices from the same physical device
        createLogicalDevice();

        createSwapChain();

        createImageViews();

        std::cout << "<<< initVulkan" << std::endl;
    }

    void mainLoop() {
        std::cout << ">>> mainLoop" << std::endl;

        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();
        }

        std::cout << "<<< mainLoop" << std::endl;
    }

    // Find `vkDestroyDebugUtilsMessengerEXT` function's address
    void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator) {
        auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
        if (func != nullptr) {
            func(instance, debugMessenger, pAllocator);
        }
    }

    void cleanup() {
        std::cout << ">>> cleanup" << std::endl;

        for (auto imageView : swapChainImageViews) {
            vkDestroyImageView(device, imageView, nullptr);
        }

        // Destroy the swap chain before device
        vkDestroySwapchainKHR(device, swapChain, nullptr);

        vkDestroyDevice(device, nullptr);

        // `vkDestroyDebugUtilsMessengerEXT` must be called before the instance is destroyed
        if (enableValidationLayers) {
            DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
        }

        // Make sure that the surface is destroyed before the instance.
        vkDestroySurfaceKHR(instance, surface, nullptr);

        // The `VkInstance` should be only destroyed right before the program exits
        vkDestroyInstance(instance, nullptr);

        glfwDestroyWindow(window);
        glfwTerminate();

        std::cout << "<<< cleanup" << std::endl;
    }
};

int main() {
    HelloTriangleApplication app;

    try {
        app.run();
    }
    catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
