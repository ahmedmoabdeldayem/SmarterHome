import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import '../services/auth_service.dart';
import '../services/mqtt_service.dart';
import 'room_screen.dart';

class HomeScreen extends StatelessWidget {
  const HomeScreen({super.key});

  static const _rooms = [
    {'id': 'living_room', 'label': 'Living Room', 'icon': Icons.weekend_outlined},
    {'id': 'bedroom',     'label': 'Bedroom',     'icon': Icons.bed_outlined},
    {'id': 'kitchen',     'label': 'Kitchen',     'icon': Icons.kitchen_outlined},
    {'id': 'entrance',    'label': 'Entrance',    'icon': Icons.door_front_door_outlined},
  ];

  @override
  Widget build(BuildContext context) {
    final mqtt = context.watch<MqttService>();
    final theme = Theme.of(context);

    return Scaffold(
      appBar: AppBar(
        title: const Text('SmarterHome'),
        actions: [
          // MQTT connection indicator
          Padding(
            padding: const EdgeInsets.only(right: 8),
            child: Icon(
              mqtt.connected ? Icons.wifi : Icons.wifi_off,
              color: mqtt.connected ? Colors.greenAccent : Colors.redAccent,
            ),
          ),
          IconButton(
            icon: const Icon(Icons.logout),
            onPressed: () => context.read<AuthService>().signOut(),
          ),
        ],
      ),
      body: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          _AlertBanner(mqtt: mqtt),
          Padding(
            padding: const EdgeInsets.fromLTRB(16, 16, 16, 8),
            child: Text('Rooms', style: theme.textTheme.titleMedium),
          ),
          Expanded(
            child: GridView.builder(
              padding: const EdgeInsets.all(16),
              gridDelegate: const SliverGridDelegateWithFixedCrossAxisCount(
                crossAxisCount: 2,
                mainAxisSpacing: 12,
                crossAxisSpacing: 12,
                childAspectRatio: 1.1,
              ),
              itemCount: _rooms.length,
              itemBuilder: (ctx, i) {
                final room = _rooms[i];
                final online = mqtt.getState('smarthome/${room['id']}/status')?['online'] == true;
                return _RoomCard(
                  label: room['label'] as String,
                  icon: room['icon'] as IconData,
                  online: online,
                  onTap: () => Navigator.push(
                    context,
                    MaterialPageRoute(
                      builder: (_) => RoomScreen(roomId: room['id'] as String, label: room['label'] as String),
                    ),
                  ),
                );
              },
            ),
          ),
        ],
      ),
    );
  }
}

class _RoomCard extends StatelessWidget {
  final String label;
  final IconData icon;
  final bool online;
  final VoidCallback onTap;

  const _RoomCard({required this.label, required this.icon, required this.online, required this.onTap});

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    return Card(
      clipBehavior: Clip.antiAlias,
      child: InkWell(
        onTap: onTap,
        child: Padding(
          padding: const EdgeInsets.all(16),
          child: Column(
            mainAxisAlignment: MainAxisAlignment.center,
            children: [
              Icon(icon, size: 40, color: theme.colorScheme.primary),
              const SizedBox(height: 10),
              Text(label, style: theme.textTheme.titleSmall),
              const SizedBox(height: 6),
              Row(
                mainAxisSize: MainAxisSize.min,
                children: [
                  Icon(
                    Icons.circle,
                    size: 8,
                    color: online ? Colors.greenAccent : Colors.grey,
                  ),
                  const SizedBox(width: 4),
                  Text(
                    online ? 'Online' : 'Offline',
                    style: theme.textTheme.bodySmall?.copyWith(
                      color: online ? Colors.greenAccent : Colors.grey,
                    ),
                  ),
                ],
              ),
            ],
          ),
        ),
      ),
    );
  }
}

class _AlertBanner extends StatelessWidget {
  final MqttService mqtt;
  const _AlertBanner({required this.mqtt});

  @override
  Widget build(BuildContext context) {
    final gasAlert = mqtt.getState('smarthome/kitchen/gas/status')?['alert'] == true;
    final motionAlert = mqtt.getState('smarthome/entrance/motion/event')?['detected'] == true;
    final doorbellAlert = mqtt.getState('smarthome/alerts/doorbell') != null;

    if (!gasAlert && !motionAlert && !doorbellAlert) return const SizedBox.shrink();

    String message = '';
    Color color = Colors.orange;
    if (gasAlert) { message = 'Gas alert in Kitchen!'; color = Colors.redAccent; }
    else if (doorbellAlert) { message = 'Someone is at the door!'; }
    else if (motionAlert) { message = 'Motion detected at entrance.'; }

    return Container(
      color: color.withOpacity(0.15),
      padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 10),
      child: Row(
        children: [
          Icon(Icons.warning_amber_rounded, color: color),
          const SizedBox(width: 8),
          Expanded(child: Text(message, style: TextStyle(color: color))),
        ],
      ),
    );
  }
}
