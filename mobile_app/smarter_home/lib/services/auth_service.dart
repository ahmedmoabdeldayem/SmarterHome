import 'package:amazon_cognito_identity_dart_2/cognito.dart';
import 'package:flutter/foundation.dart';
import 'package:flutter_secure_storage/flutter_secure_storage.dart';

// ── AWS Cognito Configuration ──────────────────────────────────────────────────
// Fill these in after running `aws cognito-idp create-user-pool` or via console
const _userPoolId = 'us-east-1_XXXXXXXXX';
const _clientId = 'XXXXXXXXXXXXXXXXXXXXXXXXXX';

class AuthService extends ChangeNotifier {
  final _storage = const FlutterSecureStorage();
  final _userPool = CognitoUserPool(_userPoolId, _clientId);

  CognitoUserSession? _session;
  String? _username;

  bool get isSignedIn => _session?.isValid() ?? false;
  String? get username => _username;
  String? get identityToken => _session?.idToken.jwtToken;

  Future<void> init() async {
    // Restore previous session from secure storage
    final user = await _userPool.getCurrentUser();
    if (user != null) {
      try {
        _session = await user.getSession();
        _username = user.username;
        notifyListeners();
      } catch (_) {
        // Session expired or invalid — stay logged out
      }
    }
  }

  Future<void> signIn(String username, String password) async {
    final cognitoUser = CognitoUser(username, _userPool, storage: _userPool.storage);
    final authDetails = AuthenticationDetails(
      username: username,
      password: password,
    );
    _session = await cognitoUser.authenticateUser(authDetails);
    _username = username;
    notifyListeners();
  }

  Future<void> signOut() async {
    final user = await _userPool.getCurrentUser();
    await user?.signOut();
    _session = null;
    _username = null;
    notifyListeners();
  }
}
