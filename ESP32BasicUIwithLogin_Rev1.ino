#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>  // Library for storing data in flash memory

// Network credentials
const char* ssid = "your-ssid-here";  
const char* password = "your-pw-here";  

// Web server instance
WebServer server(80);
Preferences preferences;  // For storing credentials

// GPIO assignments
const int output15 = 15;
const int output4 = 4;

// Output states
String output15State = "off";
String output4State = "off";

// Authentication state
bool isAuthenticated = false;

// Uptime function
String getUptime() {
  unsigned long uptimeMillis = millis();
  unsigned long seconds = uptimeMillis / 1000;
  unsigned long minutes = seconds / 60;
  unsigned long hours = minutes / 60;
  unsigned long days = hours / 24;

  seconds %= 60;
  minutes %= 60;
  hours %= 24;

  return String(days) + "d " + String(hours) + "h " + String(minutes) + "m " + String(seconds) + "s";
}

// HTML login page
const char loginPage[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head><title>Login</title></head>
<body>
  <h2>Login to ESP32</h2>
  <form action="/login" method="POST">
    Username: <input type="text" name="user"><br>
    Password: <input type="password" name="pass"><br>
    <input type="submit" value="Login">
  </form>
</body>
</html>
)rawliteral";

// HTML settings page (for changing login credentials)
const char settingsPage[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head><title>Settings</title></head>
<body>
  <h2>Update Login Credentials</h2>
  <form action="/update_credentials" method="POST">
    New Username: <input type="text" name="newUser"><br>
    New Password: <input type="password" name="newPass"><br>
    <input type="submit" value="Update">
  </form>
  <br>
  <a href="/">Back to Home</a>
</body>
</html>
)rawliteral";

// Control panel (protected)
String controlPage() {
  return R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>ESP32 Control</title>
  <style>
    html { font-family: Helvetica; text-align: center; }
    .button { background-color: #4CAF50; color: white; padding: 16px; font-size: 20px; border: none; }
    .button2 { background-color: #555; }
  </style>
  <script>

    // Function to toggle GPIO15 state via AJAX
    function toggleGPIO15() {
      var xhr = new XMLHttpRequest();
      xhr.open('GET', '/toggle?pin=15', true);  // Send GET request to the ESP32
      xhr.onreadystatechange = function() {
        if (xhr.readyState == 4 && xhr.status == 200) {
          // Update the button text based on the response
          var newState15 = xhr.responseText;
          var button = document.getElementById('gpio15Button');
          var stateText = document.getElementById('gpio15State');
          if (newState15 == 'on') {
            button.innerHTML = 'Turn off';
            stateText.innerHTML = 'Currently on';
          } else {
            button.innerHTML = 'Turn on';
            stateText.innerHTML = 'Currently off';
          }
        }
      };
      xhr.send();
    }
    // Function to toggle GPIO4 state via AJAX
function toggleGPIO4() {
  var xhr = new XMLHttpRequest();
  xhr.open('GET', '/toggle?pin=4', true);  // Send GET request to the ESP32
  xhr.onreadystatechange = function() {
    if (xhr.readyState == 4 && xhr.status == 200) {
      // Update the button text based on the response
      var states = JSON.parse(xhr.responseText); // Parse the JSON response
      var newState4 = states.gpio4;
      var button = document.getElementById('gpio4Button');
      var stateText = document.getElementById('gpio4State');
      if (newState4 == 'on') {
        button.innerHTML = 'Turn off';
        stateText.innerHTML = 'Currently on';
      } else {
        button.innerHTML = 'Turn on';
        stateText.innerHTML = 'Currently off';
      }
    }
  };
  xhr.send();
}

    // Function to update uptime without refresh
    function updateUptime() {
      var xhr = new XMLHttpRequest();
      xhr.open('GET', '/uptime', true);  // Send GET request to fetch uptime
      xhr.onreadystatechange = function() {
        if (xhr.readyState == 4 && xhr.status == 200) {
          document.getElementById('uptime').innerText = xhr.responseText;
        }
      };
      xhr.send();
    }

    // Call updateUptime every 1 seconds
    setInterval(updateUptime, 1000);


    // Function to update the GPIO states without refresh
    function updateGPIOStates() {
      var xhr = new XMLHttpRequest();
      xhr.open('GET', '/get_state', true);  // Fetch the current state of both GPIOs
      xhr.onreadystatechange = function() {
        if (xhr.readyState == 4 && xhr.status == 200) {
          var states = JSON.parse(xhr.responseText);
          document.getElementById('gpio15State').innerText = 'Currently ' + states.gpio15;
          document.getElementById('gpio4State').innerText = 'Currently ' + states.gpio4;
        }
      };
      xhr.send();
    }

    // Call updateGPIOStates every 1 second to keep the state in sync
    setInterval(updateGPIOStates, 1000);


  </script>
</head>
<body>
   <h1>ESP32 Web Server</h1>
  <p>Uptime: <span id="uptime">Loading...</span></p>

  <p>GPIO 15 - <span id="gpio15State">Currently off</span></p>
  <button id="gpio15Button" class="button" onclick="toggleGPIO15()">Turn on</button>

  <p>GPIO 4 - <span id="gpio4State">Currently off</span></p>
  <button id="gpio4Button" class="button" onclick="toggleGPIO4()">Turn on</button>

  <br><br>
  <a href='/settings'><button class='button2'>Settings</button></a>
  <br><br>
  <a href='/logout'><button class='button2'>Logout</button></a>
</body>
</html>
)rawliteral";
}

void setup() {
  Serial.begin(115200);
  pinMode(output15, OUTPUT);
  pinMode(output4, OUTPUT);
  digitalWrite(output15, LOW);
  digitalWrite(output4, LOW);

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected! IP address: " + WiFi.localIP().toString());

  // Initialize Preferences storage
  preferences.begin("auth", false);

  // Read stored credentials
  String storedUser = preferences.getString("username", "admin");
  String storedPass = preferences.getString("password", "password123");

  // Routes
  server.on("/", HTTP_GET, []() {
    if (isAuthenticated) {
      server.send(200, "text/html", controlPage());
    } else {
      server.send(200, "text/html", loginPage);
    }
  });

  // Login handler
  server.on("/login", HTTP_POST, [storedUser, storedPass]() mutable {
    if (server.hasArg("user") && server.hasArg("pass")) {
      if (server.arg("user") == storedUser && server.arg("pass") == storedPass) {
        isAuthenticated = true;
        server.sendHeader("Location", "/");
        server.send(302, "text/plain", "");
        return;
      }
    }
    server.send(401, "text/html", "<h3>Login failed! <a href='/'>Try again</a></h3>");
  });

  // Logout
  server.on("/logout", HTTP_GET, []() {
    isAuthenticated = false;
    server.sendHeader("Location", "/");
    server.send(302, "text/plain", "");
  });

  // Show settings page (only if authenticated)
  server.on("/settings", HTTP_GET, []() {
    if (!isAuthenticated) {
      server.send(403, "text/plain", "Access denied.");
      return;
    }
    server.send(200, "text/html", settingsPage);
  });

  // Update login credentials
  server.on("/update_credentials", HTTP_POST, [storedUser, storedPass]() mutable {
    if (!isAuthenticated) {
      server.send(403, "text/plain", "Access denied.");
      return;
    }
    if (server.hasArg("newUser") && server.hasArg("newPass")) {
      storedUser = server.arg("newUser");
      storedPass = server.arg("newPass");
      
      // Save new credentials to Preferences
      preferences.putString("username", storedUser);
      preferences.putString("password", storedPass);

      // Send the countdown page
        server.send(200, "text/html",
            "<!DOCTYPE html>"
            "<html>"
            "<head>"
            "<meta http-equiv='refresh' content='5;url=/'>"
            "<script>"
            "let count = 5;"
            "function countdown() {"
            "  document.getElementById('timer').innerText = count;"
            "  if (count-- > 0) {"
            "    setTimeout(countdown, 1000);"
            "  }"
            "}"
            "window.onload = countdown;"
            "</script>"
            "</head>"
            "<body style='text-align:center; font-family: Arial;'>"
            "<h2>Credentials updated!</h2>"
            "<p>Rebooting in <span id='timer'>5</span> seconds...</p>"
            "<p>You will be redirected to the login page automatically.</p>"
            "</body>"
            "</html>"
        );

        delay(5000);  // Wait for 5 seconds
        ESP.restart();  // Reboot ESP32


    } else {
      server.send(400, "text/html", "<h3>Invalid input. <a href='/settings'>Try again</a></h3>");
    }
  });

  // Toggle GPIO state
server.on("/toggle", HTTP_GET, []() {
  if (!isAuthenticated) {
    server.send(403, "text/plain", "Access denied.");
    return;
  }
  if (server.hasArg("pin")) {
    int pin = server.arg("pin").toInt();
    if (pin == 15) {
      digitalWrite(output15, !digitalRead(output15));  // Toggle GPIO 15
      output15State = digitalRead(output15) ? "on" : "off";
    } else if (pin == 4) {
      digitalWrite(output4, !digitalRead(output4));  // Toggle GPIO 4
      output4State = digitalRead(output4) ? "on" : "off";
    }
    // For AJAX, send the new state of both GPIOs (on/off)
    String response = output15State;  // Start with GPIO15 state
    if (pin == 4) {
      // When GPIO4 is toggled, return the new state for both GPIOs
      response = "{\"gpio15\":\"" + output15State + "\",\"gpio4\":\"" + output4State + "\"}";
    }
    server.send(200, "text/plain", response);  // Return the new state as a response
  } else {
    server.send(400, "text/plain", "Invalid request");
  }
});

  // Uptime endpoint for AJAX
  server.on("/uptime", HTTP_GET, []() {
    server.send(200, "text/plain", getUptime());
  });

  // Return state of GPIO
  server.on("/get_state", HTTP_GET, []() {
  String gpio15State = digitalRead(output15) == HIGH ? "on" : "off";
  String gpio4State = digitalRead(output4) == HIGH ? "on" : "off";
  
  // Send the state of both GPIOs as a JSON response
  String response = "{\"gpio15\":\"" + gpio15State + "\",\"gpio4\":\"" + gpio4State + "\"}";
  server.send(200, "application/json", response);
});

  server.begin();
}

void loop() {
  server.handleClient();
}
