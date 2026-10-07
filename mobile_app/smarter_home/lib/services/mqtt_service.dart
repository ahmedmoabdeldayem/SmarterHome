import 'dart:convert';
import 'package:flutter/foundation.dart';
import 'package:mqtt_client/mqtt_client.dart';
import 'package:mqtt_client/mqtt_server_client.dart';

// ── AWS IoT Configuration ─────────────────────────────────────────────────────
// Fill these in from aws/certs/endpoint.txt after provisioning
const _awsEndpoint = 'XXXXXXXXXXXX-ats.iot.us-east-1.amazonaws.com';
const _awsPort = 443;  // WebSocket over HTTPS
const _clientId = 'smarthome-mobile-app';

// Topic root
const _topicRoot = 'smarthome';

class MqttService extends ChangeNotifier {
  MqttServerClient? _client;
  bool _connected = false;

  // Latest known device states: topic → payload map
  final Map<String, Map<String, dynamic>> _state = {};

  bool get connected => _connected;

  Map<String, dynamic>? getState(String topic) => _state[topic];

  // ── Connect ────────────────────────────────────────────────────────────────
  Future<void> connect(String jwtToken) async {
    _client = MqttServerClient.withPort(_awsEndpoint, _clientId, _awsPort);
    _client!.useWebSocket = true;
    _client!.secure = true;
    _client!.websocketProtocols = ['mqtt'];
    _client!.keepAlivePeriod = 30;
    _client!.autoReconnect = true;
    _client!.logging(on: false);

    // AWS IoT WebSocket auth uses query-string SigV4 or Cognito JWT
    // For Cognito-based access, attach the JWT as a custom auth header
    // (Requires AWS IoT custom authorizer configured — see docs/setup.md)
    _client!.connectionMessage = MqttConnectMessage()
        .withClientIdentifier(_clientId)
        .withWillQos(MqttQos.atLeastOnce)
        .startClean();

    try {
      await _client!.connect();
      _connected = true;

      _client!.subscribe('$_topicRoot/#', MqttQos.atLeastOnce);

      _client!.updates!.listen((List<MqttReceivedMessage<MqttMessage>> messages) {
        for (final msg in messages) {
          final payload = MqttPublishPayload.bytesToStringAsString(
            (msg.payload as MqttPublishMessage).payload.message,
          );
          _handleMessage(msg.topic, payload);
        }
      });

      notifyListeners();
    } catch (e) {
      _connected = false;
      debugPrint('MQTT connection failed: $e');
      notifyListeners();
    }
  }

  void _handleMessage(String topic, String payload) {
    try {
      final decoded = json.decode(payload) as Map<String, dynamic>;
      _state[topic] = decoded;
      notifyListeners();
    } catch (_) {}
  }

  // ── Publish ────────────────────────────────────────────────────────────────
  void publish(String topic, Map<String, dynamic> payload) {
    if (_client == null || !_connected) return;

    final builder = MqttClientPayloadBuilder();
    builder.addString(json.encode(payload));
    _client!.publishMessage(topic, MqttQos.atLeastOnce, builder.payload!);
  }

  // ── Convenience helpers ────────────────────────────────────────────────────
  void setLight(String room, int channel, bool on) {
    publish('$_topicRoot/$room/light/$channel/set', {'state': on ? 'on' : 'off'});
  }

  void setAC(String room, {required String mode, int temp = 24}) {
    publish('$_topicRoot/$room/ac/set', {'mode': mode, 'temp': temp});
  }

  void setDoorLock(bool locked) {
    publish('$_topicRoot/entrance/door_lock/set', {'locked': locked});
  }

  void sendTVCommand(String cmd) {
    publish('$_topicRoot/living_room/tv/set', {'cmd': cmd});
  }

  @override
  void dispose() {
    _client?.disconnect();
    super.dispose();
  }
}
