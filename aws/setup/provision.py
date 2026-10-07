#!/usr/bin/env python3
"""
SmarterHome — AWS IoT Core provisioning script.

Creates IoT Things, certificates, and policies for each node.
Run once from your development machine with AWS credentials configured.

Usage:
    pip install boto3
    aws configure   # set your region, access key, secret key
    python provision.py
"""

import boto3
import json
import os

REGION = "us-east-1"  # Change to your preferred region
THINGS = [
    "smarthome-hub",
    "smarthome-living-room",
    "smarthome-bedroom",
    "smarthome-kitchen",
    "smarthome-entrance",
]
CERTS_DIR = os.path.join(os.path.dirname(__file__), "../certs")

POLICY_NAME = "SmarterHomePolicy"
POLICY_DOCUMENT = {
    "Version": "2012-10-17",
    "Statement": [
        {
            "Effect": "Allow",
            "Action": [
                "iot:Connect",
                "iot:Publish",
                "iot:Subscribe",
                "iot:Receive",
                "iot:GetThingShadow",
                "iot:UpdateThingShadow",
                "iot:DeleteThingShadow",
            ],
            "Resource": "*",
        }
    ],
}


def main():
    client = boto3.client("iot", region_name=REGION)
    os.makedirs(CERTS_DIR, exist_ok=True)

    # Create shared IoT policy
    try:
        client.create_policy(
            policyName=POLICY_NAME,
            policyDocument=json.dumps(POLICY_DOCUMENT),
        )
        print(f"[+] Created policy: {POLICY_NAME}")
    except client.exceptions.ResourceAlreadyExistsException:
        print(f"[~] Policy already exists: {POLICY_NAME}")

    # Download Amazon root CA
    root_ca_path = os.path.join(CERTS_DIR, "AmazonRootCA1.pem")
    if not os.path.exists(root_ca_path):
        import urllib.request
        urllib.request.urlretrieve(
            "https://www.amazontrust.com/repository/AmazonRootCA1.pem",
            root_ca_path,
        )
        print(f"[+] Downloaded Amazon Root CA → {root_ca_path}")

    # Fetch the IoT endpoint
    endpoint_response = client.describe_endpoint(endpointType="iot:Data-ATS")
    endpoint = endpoint_response["endpointAddress"]
    print(f"[i] IoT endpoint: {endpoint}")

    # Write endpoint to a config file for use by hub and firmware
    with open(os.path.join(CERTS_DIR, "endpoint.txt"), "w") as f:
        f.write(endpoint)

    # Create each Thing with its own certificate
    for thing_name in THINGS:
        thing_dir = os.path.join(CERTS_DIR, thing_name)
        os.makedirs(thing_dir, exist_ok=True)

        # Create Thing
        try:
            client.create_thing(thingName=thing_name)
            print(f"[+] Created Thing: {thing_name}")
        except client.exceptions.ResourceAlreadyExistsException:
            print(f"[~] Thing already exists: {thing_name}")

        cert_path = os.path.join(thing_dir, "certificate.pem.crt")
        key_path = os.path.join(thing_dir, "private.pem.key")

        if os.path.exists(cert_path):
            print(f"[~] Cert already exists for {thing_name}, skipping")
            continue

        # Create certificate
        cert_response = client.create_keys_and_certificate(setAsActive=True)
        cert_arn = cert_response["certificateArn"]
        cert_id = cert_response["certificateId"]

        # Save cert and private key
        with open(cert_path, "w") as f:
            f.write(cert_response["certificatePem"])
        with open(key_path, "w") as f:
            f.write(cert_response["keyPair"]["PrivateKey"])
        # Public key not needed but save for reference
        with open(os.path.join(thing_dir, "public.pem.key"), "w") as f:
            f.write(cert_response["keyPair"]["PublicKey"])

        print(f"[+] Created certificate for {thing_name}: {cert_id}")

        # Attach policy to certificate
        client.attach_policy(policyName=POLICY_NAME, target=cert_arn)
        print(f"[+] Attached policy to cert for {thing_name}")

        # Attach certificate to Thing
        client.attach_thing_principal(thingName=thing_name, principal=cert_arn)
        print(f"[+] Attached cert to Thing {thing_name}")

        print(f"    Cert  → {cert_path}")
        print(f"    Key   → {key_path}")

    print("\n[✓] Provisioning complete.")
    print(f"[i] Endpoint saved to: {os.path.join(CERTS_DIR, 'endpoint.txt')}")
    print("[!] Keep private keys secure — never commit them to git.")


if __name__ == "__main__":
    main()
