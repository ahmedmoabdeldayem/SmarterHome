#!/usr/bin/env python3
"""
SmarterHome — AWS IoT teardown script.

Deletes all Things, certs, and the shared policy created by provision.py.
Use with caution — this is irreversible.

Usage:
    python teardown.py
"""

import boto3
import json

REGION = "us-east-1"
THINGS = [
    "smarthome-hub",
    "smarthome-living-room",
    "smarthome-bedroom",
    "smarthome-kitchen",
    "smarthome-entrance",
]
POLICY_NAME = "SmarterHomePolicy"


def main():
    client = boto3.client("iot", region_name=REGION)

    for thing_name in THINGS:
        print(f"[~] Cleaning up Thing: {thing_name}")

        # List and detach principals (certs) from thing
        try:
            principals = client.list_thing_principals(thingName=thing_name)["principals"]
        except client.exceptions.ResourceNotFoundException:
            print(f"    Thing not found, skipping.")
            continue

        for principal_arn in principals:
            cert_id = principal_arn.split("/")[-1]

            # Detach from thing
            client.detach_thing_principal(thingName=thing_name, principal=principal_arn)

            # Detach policy
            try:
                client.detach_policy(policyName=POLICY_NAME, target=principal_arn)
            except Exception:
                pass

            # Deactivate and delete cert
            client.update_certificate(certificateId=cert_id, newStatus="INACTIVE")
            client.delete_certificate(certificateId=cert_id, forceDelete=True)
            print(f"    Deleted cert: {cert_id}")

        # Delete thing
        client.delete_thing(thingName=thing_name)
        print(f"    Deleted Thing: {thing_name}")

    # Delete policy
    try:
        client.delete_policy(policyName=POLICY_NAME)
        print(f"[+] Deleted policy: {POLICY_NAME}")
    except client.exceptions.ResourceNotFoundException:
        pass

    print("[✓] Teardown complete.")


if __name__ == "__main__":
    confirm = input("This will delete all SmarterHome AWS resources. Type 'yes' to confirm: ")
    if confirm.strip().lower() == "yes":
        main()
    else:
        print("Aborted.")
