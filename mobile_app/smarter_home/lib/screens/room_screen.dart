import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import '../services/mqtt_service.dart';

class RoomScreen extends StatelessWidget {
  final String roomId;
  final String label;

  const RoomScreen({super.key, required this.roomId, required this.label});

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: Text(label)),
      body: SingleChildScrollView(
        padding: const EdgeInsets.all(16),
        child: switch (roomId) {
          'living_room' => const _LivingRoomControls(),
          'bedroom'     => const _BedroomControls(),
          'kitchen'     => const _KitchenControls(),
          'entrance'    => const _EntranceControls(),
          _             => const Center(child: Text('Unknown room')),
        },
      ),
    );
  }
}

// ── Living Room ───────────────────────────────────────────────────────────────
class _LivingRoomControls extends StatelessWidget {
  const _LivingRoomControls();

  @override
  Widget build(BuildContext context) {
    final mqtt = context.watch<MqttService>();
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        _SectionHeader('Lighting'),
        for (int i = 1; i <= 4; i++)
          _LightTile(
            label: 'Light $i',
            topic: 'smarthome/living_room/light/$i/status',
            onToggle: (on) => mqtt.setLight('living_room', i, on),
          ),
        const SizedBox(height: 16),
        _SectionHeader('Climate (AC)'),
        _ACControls(room: 'living_room'),
        const SizedBox(height: 16),
        _SectionHeader('TV'),
        _TVControls(),
        const SizedBox(height: 16),
        _SectionHeader('Energy'),
        _EnergyTile(topic: 'smarthome/living_room/energy/status'),
      ],
    );
  }
}

// ── Bedroom ───────────────────────────────────────────────────────────────────
class _BedroomControls extends StatelessWidget {
  const _BedroomControls();

  @override
  Widget build(BuildContext context) {
    final mqtt = context.watch<MqttService>();
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        _SectionHeader('Lighting'),
        for (int i = 1; i <= 2; i++)
          _LightTile(
            label: 'Light $i',
            topic: 'smarthome/bedroom/light/$i/status',
            onToggle: (on) => mqtt.setLight('bedroom', i, on),
          ),
        const SizedBox(height: 16),
        _SectionHeader('Climate (AC)'),
        _ACControls(room: 'bedroom'),
        const SizedBox(height: 16),
        _SectionHeader('Temperature & Humidity'),
        _ClimateSensor(topic: 'smarthome/bedroom/climate/status'),
      ],
    );
  }
}

// ── Kitchen ───────────────────────────────────────────────────────────────────
class _KitchenControls extends StatelessWidget {
  const _KitchenControls();

  @override
  Widget build(BuildContext context) {
    final mqtt = context.watch<MqttService>();
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        _SectionHeader('Lighting'),
        _LightTile(
          label: 'Kitchen Light',
          topic: 'smarthome/kitchen/light/status',
          onToggle: (on) => mqtt.setLight('kitchen', 1, on),
        ),
        const SizedBox(height: 16),
        _SectionHeader('Gas / Safety'),
        _GasSensor(topic: 'smarthome/kitchen/gas/status'),
        const SizedBox(height: 16),
        _SectionHeader('Energy'),
        _EnergyTile(topic: 'smarthome/kitchen/energy/status'),
      ],
    );
  }
}

// ── Entrance ──────────────────────────────────────────────────────────────────
class _EntranceControls extends StatelessWidget {
  const _EntranceControls();

  @override
  Widget build(BuildContext context) {
    final mqtt = context.watch<MqttService>();
    final lockState = mqtt.getState('smarthome/entrance/door_lock/status');
    final isLocked = lockState?['locked'] != false;
    final motionState = mqtt.getState('smarthome/entrance/motion/event');

    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        _SectionHeader('Door Lock'),
        Card(
          child: ListTile(
            leading: Icon(
              isLocked ? Icons.lock : Icons.lock_open,
              color: isLocked ? Colors.greenAccent : Colors.orangeAccent,
            ),
            title: Text(isLocked ? 'Locked' : 'Unlocked'),
            subtitle: const Text('Tap to toggle'),
            trailing: Switch(
              value: !isLocked,
              onChanged: (unlocked) => mqtt.setDoorLock(!unlocked),
            ),
          ),
        ),
        const SizedBox(height: 16),
        _SectionHeader('Motion'),
        Card(
          child: ListTile(
            leading: const Icon(Icons.motion_photos_on_outlined),
            title: Text(
              motionState?['detected'] == true
                  ? 'Motion detected!'
                  : 'No motion',
            ),
          ),
        ),
      ],
    );
  }
}

// ── Shared Widgets ────────────────────────────────────────────────────────────
class _SectionHeader extends StatelessWidget {
  final String title;
  const _SectionHeader(this.title);

  @override
  Widget build(BuildContext context) => Padding(
    padding: const EdgeInsets.only(bottom: 8),
    child: Text(title, style: Theme.of(context).textTheme.titleSmall?.copyWith(
      color: Theme.of(context).colorScheme.primary,
    )),
  );
}

class _LightTile extends StatelessWidget {
  final String label;
  final String topic;
  final ValueChanged<bool> onToggle;

  const _LightTile({required this.label, required this.topic, required this.onToggle});

  @override
  Widget build(BuildContext context) {
    final state = context.watch<MqttService>().getState(topic);
    final isOn = state?['state'] == 'on';

    return Card(
      child: SwitchListTile(
        title: Text(label),
        secondary: Icon(isOn ? Icons.lightbulb : Icons.lightbulb_outline),
        value: isOn,
        onChanged: onToggle,
      ),
    );
  }
}

class _ACControls extends StatefulWidget {
  final String room;
  const _ACControls({required this.room});

  @override
  State<_ACControls> createState() => _ACControlsState();
}

class _ACControlsState extends State<_ACControls> {
  String _mode = 'off';
  int _temp = 24;

  @override
  Widget build(BuildContext context) {
    final mqtt = context.read<MqttService>();
    return Card(
      child: Padding(
        padding: const EdgeInsets.all(12),
        child: Column(
          children: [
            Row(
              mainAxisAlignment: MainAxisAlignment.spaceEvenly,
              children: [
                for (final mode in ['cool', 'heat', 'fan', 'off'])
                  ChoiceChip(
                    label: Text(mode.toUpperCase()),
                    selected: _mode == mode,
                    onSelected: (_) {
                      setState(() => _mode = mode);
                      mqtt.setAC(widget.room, mode: mode, temp: _temp);
                    },
                  ),
              ],
            ),
            if (_mode != 'off') ...[
              const SizedBox(height: 8),
              Row(
                children: [
                  const Icon(Icons.thermostat_outlined),
                  Expanded(
                    child: Slider(
                      value: _temp.toDouble(),
                      min: 16,
                      max: 30,
                      divisions: 14,
                      label: '$_temp°C',
                      onChanged: (v) => setState(() => _temp = v.round()),
                      onChangeEnd: (v) => mqtt.setAC(widget.room, mode: _mode, temp: v.round()),
                    ),
                  ),
                  Text('$_temp°C'),
                ],
              ),
            ],
          ],
        ),
      ),
    );
  }
}

class _TVControls extends StatelessWidget {
  @override
  Widget build(BuildContext context) {
    final mqtt = context.read<MqttService>();
    return Card(
      child: Padding(
        padding: const EdgeInsets.symmetric(vertical: 8),
        child: Row(
          mainAxisAlignment: MainAxisAlignment.spaceEvenly,
          children: [
            IconButton(icon: const Icon(Icons.power_settings_new), onPressed: () => mqtt.sendTVCommand('on')),
            IconButton(icon: const Icon(Icons.volume_up), onPressed: () => mqtt.sendTVCommand('vol_up')),
            IconButton(icon: const Icon(Icons.volume_down), onPressed: () => mqtt.sendTVCommand('vol_down')),
            IconButton(icon: const Icon(Icons.volume_off), onPressed: () => mqtt.sendTVCommand('mute')),
          ],
        ),
      ),
    );
  }
}

class _EnergyTile extends StatelessWidget {
  final String topic;
  const _EnergyTile({required this.topic});

  @override
  Widget build(BuildContext context) {
    final state = context.watch<MqttService>().getState(topic);
    final watts = state?['watts']?.toString() ?? '—';
    final amps = state?['amps']?.toString() ?? '—';

    return Card(
      child: ListTile(
        leading: const Icon(Icons.bolt),
        title: Text('$watts W'),
        subtitle: Text('$amps A'),
      ),
    );
  }
}

class _ClimateSensor extends StatelessWidget {
  final String topic;
  const _ClimateSensor({required this.topic});

  @override
  Widget build(BuildContext context) {
    final state = context.watch<MqttService>().getState(topic);
    final temp = state?['temp']?.toString() ?? '—';
    final humidity = state?['humidity']?.toString() ?? '—';

    return Card(
      child: ListTile(
        leading: const Icon(Icons.thermostat),
        title: Text('$temp°C'),
        subtitle: Text('Humidity: $humidity%'),
      ),
    );
  }
}

class _GasSensor extends StatelessWidget {
  final String topic;
  const _GasSensor({required this.topic});

  @override
  Widget build(BuildContext context) {
    final state = context.watch<MqttService>().getState(topic);
    final ppm = state?['ppm']?.toString() ?? '—';
    final alert = state?['alert'] == true;

    return Card(
      color: alert ? Colors.redAccent.withOpacity(0.2) : null,
      child: ListTile(
        leading: Icon(Icons.gas_meter_outlined, color: alert ? Colors.redAccent : null),
        title: Text('$ppm PPM'),
        subtitle: Text(alert ? 'ALERT: Gas detected!' : 'Normal'),
      ),
    );
  }
}
