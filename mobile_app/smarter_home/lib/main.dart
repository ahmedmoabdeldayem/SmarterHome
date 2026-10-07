import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import 'services/mqtt_service.dart';
import 'services/auth_service.dart';
import 'screens/login_screen.dart';
import 'screens/home_screen.dart';

void main() async {
  WidgetsFlutterBinding.ensureInitialized();

  final authService = AuthService();
  await authService.init();

  runApp(
    MultiProvider(
      providers: [
        ChangeNotifierProvider(create: (_) => authService),
        ChangeNotifierProvider(create: (_) => MqttService()),
      ],
      child: const SmarterHomeApp(),
    ),
  );
}

class SmarterHomeApp extends StatelessWidget {
  const SmarterHomeApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'SmarterHome',
      debugShowCheckedModeBanner: false,
      theme: ThemeData(
        colorScheme: ColorScheme.fromSeed(
          seedColor: const Color(0xFF1A73E8),
          brightness: Brightness.dark,
        ),
        useMaterial3: true,
      ),
      home: Consumer<AuthService>(
        builder: (context, auth, _) {
          return auth.isSignedIn ? const HomeScreen() : const LoginScreen();
        },
      ),
    );
  }
}
