#define BUILD_VERSION "1.0.0"

#define UPDATE_CHECK_INTERVAL 86400000 // ONE DAY
#define HTTP_REQUEST_INTERVAL 2000
#define HTTP_REQUEST_TIMEOUT 10000
#define MAXIMUM_DISCONNECT_TIME 900000 // Maximum WiFi disconnection time or server request time out before rollback to AP Mode and reset FB

static const char baseUri[] = "192.168.2.130";
static const char espUpdaterPath[] = "/otoma/api/ESPUpdater.php";
static const char requestURL[] = "/otoma/api/nitenanControllerRequest.php";
static const char identifyURL[] = "/otoma/api/identifyDevice.php";
static const int serverPort = 8080;

// CAMERA_MODEL_AI_THINKER
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27

#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22