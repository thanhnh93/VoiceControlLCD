/*
   ESP32 SMART HOME VOICE CONTROL

   ESP32 DevKit V1

   Relay đèn  -> GPIO26
   Relay quạt -> GPIO27
   Servo SG90 -> GPIO18

   LCD I2C
   SDA -> GPIO21
   SCL -> GPIO22

   Voice Control by Web Speech API
   Preferences save commands
*/

#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>
#include <Preferences.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>


// =====================================================
// WIFI
// =====================================================

const char* ssid = "A34";
const char* password = "12345678";


// =====================================================
// CHÂN THIẾT BỊ
// =====================================================

#define LED_RELAY 26
#define FAN_RELAY 27
#define SERVO_PIN 18

#define SDA_PIN 21
#define SCL_PIN 22


// =====================================================
// LCD
// =====================================================

LiquidCrystal_I2C lcd(0x27, 16, 2);


// =====================================================
// OBJECT
// =====================================================

WebServer server(80);

Preferences prefs;

Servo doorServo;


// =====================================================
// TRẠNG THÁI
// =====================================================

bool ledState = false;
bool fanState = false;
bool doorState = false;


// =====================================================
// CÁC CÂU LỆNH LƯU TRONG EEPROM
// =====================================================

String cmdLedOn;
String cmdLedOff;

String cmdFanOn;
String cmdFanOff;

String cmdDoorOpen;
String cmdDoorClose;


// =====================================================
// LCD
// =====================================================

void hienThiLCD(String dong1, String dong2)
{
  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print(dong1);

  lcd.setCursor(0, 1);
  lcd.print(dong2);
}


// =====================================================
// ĐỌC CÁC LỆNH ĐÃ LƯU
// =====================================================

void loadCommands()
{
  prefs.begin("voice", false);

  cmdLedOn =
    prefs.getString("ledOn", "Bật đèn");

  cmdLedOff =
    prefs.getString("ledOff", "Tắt đèn");

  cmdFanOn =
    prefs.getString("fanOn", "Bật quạt");

  cmdFanOff =
    prefs.getString("fanOff", "Tắt quạt");

  cmdDoorOpen =
    prefs.getString("doorOn", "Mở cửa");

  cmdDoorClose =
    prefs.getString("doorOff", "Đóng cửa");
}


// =====================================================
// CẬP NHẬT OUTPUT
// =====================================================

void updateOutputs()
{
  // Relay đèn
  digitalWrite(LED_RELAY, ledState);

  // Relay quạt
  digitalWrite(FAN_RELAY, fanState);

  // Servo cửa
  if (doorState)
  {
    doorServo.write(90);
  }
  else
  {
    doorServo.write(0);
  }


  // Serial
  Serial.println();
  Serial.println("==============================");

  Serial.print("DEN  : ");
  Serial.println(ledState ? "BAT" : "TAT");

  Serial.print("QUAT : ");
  Serial.println(fanState ? "BAT" : "TAT");

  Serial.print("CUA  : ");
  Serial.println(doorState ? "MO" : "DONG");

  Serial.println("==============================");
}


// =====================================================
// GIAO DIỆN WEB
// =====================================================

String getHTML()
{
  String page = R"rawliteral(

<!DOCTYPE html>
<html>

<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">

<title>BẢNG ĐIỀU KHIỂN</title>

<style>

body
{
    font-family: Arial;
    text-align: center;
    padding: 20px;
    background: #f5f5f5;
}

h2
{
    color: #333;
}

button
{
    width: 220px;
    padding: 15px;
    font-size: 18px;
    margin: 7px;
    cursor: pointer;
    border-radius: 8px;
    border: none;
}

.green
{
    background: #00cc66;
    color: white;
}

.red
{
    background: #ff4444;
    color: white;
}

.blue
{
    background: #008CBA;
    color: white;
}

.gray
{
    background: #777;
    color: white;
}

.guide-box
{
    background: white;
    padding: 15px;
    border-radius: 8px;
    display: inline-block;
    text-align: left;
    margin-top: 10px;
}

#speech
{
    min-height: 30px;
    font-size: 18px;
    font-weight: bold;
    max-width: 450px;
    margin: 15px auto;
    word-wrap: break-word;
}

#status
{
    background: white;
    padding: 15px;
    border-radius: 8px;
    margin-top: 15px;
    display: inline-block;
}

</style>

</head>


<body>


<h2>🏠 ESP32 SMART HOME</h2>


<!-- ================================================= -->
<!-- ĐIỀU KHIỂN THỦ CÔNG -->
<!-- ================================================= -->

<h3>🎛️ Điều Khiển Thủ Công</h3>


<button id="btnLed"
        onclick="toggleDevice('led')">

    💡 BẬT ĐÈN

</button>

<br>


<button id="btnFan"
        onclick="toggleDevice('fan')">

    🌀 BẬT QUẠT

</button>


<hr>


<!-- ================================================= -->
<!-- ĐIỀU KHIỂN GIỌNG NÓI -->
<!-- ================================================= -->

<h3>🎤 Điều Khiển Bằng Giọng Nói</h3>


<button id="btnVoice"
        class="blue"
        onclick="startVoice()">

    🎤 BẮT ĐẦU NÓI

</button>


<p id="speech">

    Nhấn nút rồi nói lệnh

</p>


<hr>


<!-- ================================================= -->
<!-- HƯỚNG DẪN -->
<!-- ================================================= -->

<h3>📢 Lệnh Giọng Nói</h3>


<div class="guide-box">

<p>
<strong>Đèn:</strong>
"Bật đèn" / "Tắt đèn"
</p>

<p>
<strong>Quạt:</strong>
"Bật quạt" / "Tắt quạt"
</p>

</div>


<!-- ================================================= -->
<!-- TRẠNG THÁI -->
<!-- ================================================= -->

<h3>📊 Trạng Thái</h3>


<div id="status">

    Đang tải...

</div>


<script>


// =====================================================
// BIẾN TRẠNG THÁI
// =====================================================

let led = false;

let fan = false;


// =====================================================
// HIỂN THỊ NÚT
// =====================================================

function refreshButtons()
{

    // ----------------------------
    // ĐÈN
    // ----------------------------

    let btnLed =
        document.getElementById("btnLed");

    if (led)
    {
        btnLed.innerHTML =
            "💡 TẮT ĐÈN";

        btnLed.className = "red";
    }
    else
    {
        btnLed.innerHTML =
            "💡 BẬT ĐÈN";

        btnLed.className = "green";
    }


    // ----------------------------
    // QUẠT
    // ----------------------------

    let btnFan =
        document.getElementById("btnFan");

    if (fan)
    {
        btnFan.innerHTML =
            "🌀 TẮT QUẠT";

        btnFan.className = "red";
    }
    else
    {
        btnFan.innerHTML =
            "🌀 BẬT QUẠT";

        btnFan.className = "green";
    }


    // ----------------------------
    // TRẠNG THÁI
    // ----------------------------

    document.getElementById("status").innerHTML =

        "💡 Đèn: " +
        (led ? "<b>ĐANG BẬT</b>" : "ĐANG TẮT") +

        "<br><br>" +

        "🌀 Quạt: " +
        (fan ? "<b>ĐANG BẬT</b>" : "ĐANG TẮT");

}


// =====================================================
// ĐIỀU KHIỂN THỦ CÔNG
// =====================================================

function toggleDevice(dev)
{

    fetch(
        "/toggle?dev=" +
        encodeURIComponent(dev) +
        "&t=" +
        Date.now()
    )

    .then(response => response.text())

    .then(data =>
    {

        if (dev == "led")
        {
            led = !led;
        }

        if (dev == "fan")
        {
            fan = !fan;
        }

        refreshButtons();

    })

    .catch(error =>
    {

        document.getElementById("speech").innerHTML =
            "❌ Không kết nối được ESP32";

        console.log(error);

    });

}


// =====================================================
// CHUYỂN CHUỖI THÀNH KHÔNG DẤU
// =====================================================

function removeVietnameseAccent(str)
{

    return str

        .normalize("NFD")

        .replace(/[\u0300-\u036f]/g, "")

        .replace(/đ/g, "d")

        .replace(/Đ/g, "D")

        .toLowerCase()

        .trim();

}


// =====================================================
// GỬI LỆNH ĐẾN ESP32
// =====================================================

function sendVoiceCommand(command, originalText)
{

    document.getElementById("speech").innerHTML =

        "📡 Đang gửi lệnh: " +
        originalText;


    fetch(
        "/voice?cmd=" +
        encodeURIComponent(command) +
        "&t=" +
        Date.now()
    )

    .then(response =>
    {

        if (!response.ok)
        {
            throw new Error("ESP32 error");
        }

        return response.text();

    })

    .then(data =>
    {

        document.getElementById("speech").innerHTML =

            "✅ Đã thực hiện: " +
            originalText;


        // Cập nhật trạng thái sau khi thực hiện lệnh

        setTimeout(
            updateStatusFromESP,
            200
        );

    })

    .catch(error =>
    {

        document.getElementById("speech").innerHTML =
            "❌ Không gửi được lệnh đến ESP32";

        console.log(error);

    });

}


// =====================================================
// ĐỌC TRẠNG THÁI TỪ ESP32
// =====================================================

function updateStatusFromESP()
{

    fetch(
        "/status?t=" +
        Date.now()
    )

    .then(response =>
        response.json()
    )

    .then(data =>
    {

        led = data.led;

        fan = data.fan;

        refreshButtons();

    })

    .catch(error =>
    {

        console.log(error);

    });

}


// =====================================================
// 🎤 THỰC THI NHẬN DẠNG GIỌNG NÓI
// =====================================================

function executeRecognition()
{

    const SpeechRecognition =
        window.SpeechRecognition ||
        window.webkitSpeechRecognition;


    if (!SpeechRecognition)
    {

        document.getElementById("speech").innerHTML =
            "❌ Trình duyệt không hỗ trợ nhận dạng giọng nói";

        return;

    }


    const rec =
        new SpeechRecognition();


    rec.lang = "vi-VN";

    rec.continuous = false;

    rec.interimResults = false;

    rec.maxAlternatives = 3;


    // ----------------------------
    // BẮT ĐẦU NGHE
    // ----------------------------

    rec.onstart = function()
    {

        document.getElementById("btnVoice").innerHTML =
            "🔴 ĐANG NGHE...";


        document.getElementById("btnVoice").className =
            "red";


        document.getElementById("speech").innerHTML =
            "🎤 Hãy nói: bật đèn, tắt đèn, bật quạt hoặc tắt quạt";

    };


    // ----------------------------
    // NHẬN KẾT QUẢ
    // ----------------------------

    rec.onresult = function(event)
    {

        let text =
            event.results[0][0].transcript;


        text = text.trim();


        console.log(
            "Câu nhận được:",
            text
        );


        document.getElementById("speech").innerHTML =
            "🗣️ Bạn nói: <b>" +
            text +
            "</b>";


        let cmd =
            removeVietnameseAccent(text);


        console.log(
            "Câu sau khi chuẩn hóa:",
            cmd
        );


        if (cmd.includes("bat den"))
        {

            sendVoiceCommand(
                "bat den",
                text
            );

        }

        else if (cmd.includes("tat den"))
        {

            sendVoiceCommand(
                "tat den",
                text
            );

        }

        else if (cmd.includes("bat quat"))
        {

            sendVoiceCommand(
                "bat quat",
                text
            );

        }

        else if (cmd.includes("tat quat"))
        {

            sendVoiceCommand(
                "tat quat",
                text
            );

        }

        else
        {

            document.getElementById("speech").innerHTML =

                "❌ Không đúng từ khóa.<br>" +

                "Bạn nói: <b>" +
                text +
                "</b><br><br>" +

                "Hãy nói: bật đèn, tắt đèn, bật quạt hoặc tắt quạt";

        }

    };


    // ----------------------------
    // LỖI
    // ----------------------------

    rec.onerror = function(event)
    {

        console.log(
            "Speech error:",
            event.error
        );


        if (
            event.error == "not-allowed" ||
            event.error == "service-not-allowed"
        )
        {

            document.getElementById("speech").innerHTML =

                "❌ Trình duyệt chặn Micro (HTTP IP)!<br>" +
                "<small>Vui lòng mở <b>chrome://flags/#unsafely-treat-insecure-origin-as-secure</b>, bật <b>Enabled</b> và thêm IP ESP32 vào danh sách.</small>";

        }


        else if (
            event.error == "no-speech"
        )
        {

            document.getElementById("speech").innerHTML =

                "❌ Không nghe thấy giọng nói";

        }


        else if (
            event.error == "network"
        )
        {

            document.getElementById("speech").innerHTML =

                "❌ Lỗi kết nối mạng (Web Speech API cần kết nối Internet)";

        }


        else
        {

            document.getElementById("speech").innerHTML =

                "❌ Lỗi: " +
                event.error;

        }

    };


    // ----------------------------
    // KẾT THÚC
    // ----------------------------

    rec.onend = function()
    {

        document.getElementById("btnVoice").innerHTML =
            "🎤 BẮT ĐẦU NÓI";


        document.getElementById("btnVoice").className =
            "blue";

    };


    try
    {

        rec.start();

    }

    catch(error)
    {

        console.log(error);

        document.getElementById("speech").innerHTML =
            "❌ Không thể bắt đầu nhận dạng";

    }

}


// =====================================================
// 🎤 YÊU CẦU QUYỀN MICRO & BẮT ĐẦU NÓI
// =====================================================

function startVoice()
{

    // Yêu cầu cấp quyền Micro trước từ trình duyệt
    if (navigator.mediaDevices && navigator.mediaDevices.getUserMedia)
    {

        navigator.mediaDevices.getUserMedia({ audio: true })

        .then(function(stream) {

            // Dừng stream microphone ngay sau khi được cấp quyền
            stream.getTracks().forEach(track => track.stop());

            // Tiến hành nhận diện giọng nói
            executeRecognition();

        })

        .catch(function(err) {

            console.log("MediaDevice error: ", err);

            document.getElementById("speech").innerHTML =

                "❌ Trình duyệt từ chối Micro hoặc chưa bật Ngoại lệ HTTP IP.<br>" +
                "<small>Truy cập <b>chrome://flags/#unsafely-treat-insecure-origin-as-secure</b> để bỏ chặn.</small>";

        });

    }

    else
    {

        // Thử chạy trực tiếp nếu trình duyệt cũ không hỗ trợ getUserMedia
        executeRecognition();

    }

}


// =====================================================
// KHỞI ĐỘNG
// =====================================================

refreshButtons();

updateStatusFromESP();


</script>


</body>

</html>

)rawliteral";


  return page;
}


// =====================================================
// TRANG CHỦ
// =====================================================

void handleRoot()
{
  server.send(
    200,
    "text/html; charset=UTF-8",
    getHTML()
  );
}


// =====================================================
// NÚT THỦ CÔNG
// =====================================================

void handleToggle()
{

  String dev =
    server.arg("dev");


  if (dev == "led")
  {

    ledState = !ledState;

    Serial.println(
      "NUT NHAN: DEN"
    );

  }


  else if (dev == "fan")
  {

    fanState = !fanState;

    Serial.println(
      "NUT NHAN: QUAT"
    );

  }


  else if (dev == "door")
  {

    doorState = !doorState;

    Serial.println(
      "NUT NHAN: CUA"
    );

  }


  updateOutputs();


  server.send(
    200,
    "text/plain",
    "OK"
  );
}


// =====================================================
// CHUẨN HÓA CHUỖI NHẬN ĐƯỢC
// =====================================================

String normalizeString(String str)
{

  str.trim();

  str.toLowerCase();


  // Xóa một số dấu câu

  str.replace(".", "");
  str.replace(",", "");
  str.replace("!", "");
  str.replace("?", "");
  str.replace("\"", "");
  str.replace("'", "");


  return str;
}


// =====================================================
// XỬ LÝ GIỌNG NÓI
// =====================================================

void handleVoice()
{

  if (!server.hasArg("cmd"))
  {

    server.send(
      400,
      "text/plain",
      "NO COMMAND"
    );

    return;
  }


  String rawCmd =
    server.arg("cmd");


  String cmd =
    normalizeString(rawCmd);


  Serial.println();
  Serial.println("----------------------------------");

  Serial.print(
    "GIONG NOI GOC : "
  );

  Serial.println(rawCmd);

  Serial.print(
    "LENH NHAN DUOC: "
  );

  Serial.println(cmd);


  bool recognized = false;


  // =================================================
  // 💡 ĐÈN
  // =================================================


  // BẬT ĐÈN

  if (
    cmd.indexOf("bật đèn") != -1 ||
    cmd.indexOf("bat den") != -1
  )
  {

    ledState = true;

    recognized = true;

    Serial.println(
      "-> LENH: BAT DEN"
    );

  }


  // TẮT ĐÈN

  else if (
    cmd.indexOf("tắt đèn") != -1 ||
    cmd.indexOf("tat den") != -1
  )
  {

    ledState = false;

    recognized = true;

    Serial.println(
      "-> LENH: TAT DEN"
    );

  }


  // =================================================
  // 🌀 QUẠT
  // =================================================


  // BẬT QUẠT

  else if (
    cmd.indexOf("bật quạt") != -1 ||
    cmd.indexOf("bat quat") != -1
  )
  {

    fanState = true;

    recognized = true;

    Serial.println(
      "-> LENH: BAT QUAT"
    );

  }


  // TẮT QUẠT

  else if (
    cmd.indexOf("tắt quạt") != -1 ||
    cmd.indexOf("tat quat") != -1
  )
  {

    fanState = false;

    recognized = true;

    Serial.println(
      "-> LENH: TAT QUAT"
    );

  }


  // =================================================
  // 🚪 CỬA
  // =================================================


  // MỞ CỬA

  else if (
    cmd.indexOf("mở cửa") != -1 ||
    cmd.indexOf("mo cua") != -1
  )
  {

    doorState = true;

    recognized = true;

    Serial.println(
      "-> LENH: MO CUA"
    );

  }


  // ĐÓNG CỬA

  else if (
    cmd.indexOf("đóng cửa") != -1 ||
    cmd.indexOf("dong cua") != -1
  )
  {

    doorState = false;

    recognized = true;

    Serial.println(
      "-> LENH: DONG CUA"
    );

  }


  // =================================================
  // CẬP NHẬT OUTPUT
  // =================================================

  if (recognized)
  {

    updateOutputs();

    server.send(
      200,
      "text/plain",
      "VOICE OK"
    );

  }

  else
  {

    Serial.println(
      "-> KHONG NHAN DIEN LENH"
    );

    server.send(
      400,
      "text/plain",
      "UNKNOWN COMMAND"
    );

  }
}


// =====================================================
// TRẠNG THÁI
// =====================================================

void handleStatus()
{

  String json = "{";


  json += "\"led\":";

  json +=
    ledState ? "true" : "false";


  json += ",";


  json += "\"fan\":";

  json +=
    fanState ? "true" : "false";


  json += ",";


  json += "\"door\":";

  json +=
    doorState ? "true" : "false";


  json += "}";


  server.send(
    200,
    "application/json",
    json
  );
}


// =====================================================
// LƯU LỆNH
// =====================================================

void handleSave()
{

  if (server.hasArg("ledOn"))
  {
    prefs.putString(
      "ledOn",
      server.arg("ledOn")
    );
  }


  if (server.hasArg("ledOff"))
  {
    prefs.putString(
      "ledOff",
      server.arg("ledOff")
    );
  }


  if (server.hasArg("fanOn"))
  {
    prefs.putString(
      "fanOn",
      server.arg("fanOn")
    );
  }


  if (server.hasArg("fanOff"))
  {
    prefs.putString(
      "fanOff",
      server.arg("fanOff")
    );
  }


  if (server.hasArg("doorOn"))
  {
    prefs.putString(
      "doorOn",
      server.arg("doorOn")
    );
  }


  if (server.hasArg("doorOff"))
  {
    prefs.putString(
      "doorOff",
      server.arg("doorOff")
    );
  }


  loadCommands();


  Serial.println(
    "DA LUU LENH MOI"
  );


  server.send(
    200,
    "text/plain",
    "Saved"
  );
}


// =====================================================
// SETUP
// =====================================================

void setup()
{

  Serial.begin(115200);


  // =================================================
  // LCD
  // =================================================

  Wire.begin(
    SDA_PIN,
    SCL_PIN
  );


  lcd.init();

  lcd.backlight();


  hienThiLCD(
    "ESP32",
    "Khoi dong..."
  );


  delay(1000);


  // =================================================
  // GPIO
  // =================================================

  pinMode(
    LED_RELAY,
    OUTPUT
  );


  pinMode(
    FAN_RELAY,
    OUTPUT
  );


  // =================================================
  // SERVO
  // =================================================

  doorServo.attach(
    SERVO_PIN
  );


  // =================================================
  // LOAD COMMANDS
  // =================================================

  loadCommands();


  // =================================================
  // TRẠNG THÁI BAN ĐẦU
  // =================================================

  updateOutputs();


  // =================================================
  // WIFI
  // =================================================

  hienThiLCD(
    "WIFI",
    "Dang ket noi..."
  );


  Serial.println(
    "Dang ket noi WiFi..."
  );


  WiFi.begin(
    ssid,
    password
  );


  while (
    WiFi.status() != WL_CONNECTED
  )
  {

    delay(500);

    Serial.print(".");
  }


  Serial.println();

  Serial.println(
    "WiFi Connected"
  );


  Serial.print(
    "IP: "
  );

  Serial.println(
    WiFi.localIP()
  );


  // =================================================
  // LCD HIỂN THỊ IP
  // =================================================

  lcd.clear();


  lcd.setCursor(
    0,
    0
  );

  lcd.print(
    "WIFI: OK"
  );


  lcd.setCursor(
    0,
    1
  );

  lcd.print(
    WiFi.localIP()
  );


  // =================================================
  // WEB SERVER
  // =================================================

  server.on(
    "/",
    handleRoot
  );


  server.on(
    "/toggle",
    handleToggle
  );


  server.on(
    "/voice",
    handleVoice
  );


  server.on(
    "/status",
    handleStatus
  );


  server.on(
    "/save",
    handleSave
  );


  server.begin();


  Serial.println(
    "Web Server Started"
  );
}


// =====================================================
// LOOP
// =====================================================

void loop()
{

  server.handleClient();

}