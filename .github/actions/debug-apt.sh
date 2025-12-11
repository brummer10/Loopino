#!/usr/bin/env bash
set -euo pipefail

export DEBIAN_FRONTEND=noninteractive

echo "=== APT / DPKG debug: $(date) ==="

echo -e "\n--- /etc/apt/sources.list ---"
cat /etc/apt/sources.list || true

echo -e "\n--- /etc/apt/sources.list.d ---"
ls -la /etc/apt/sources.list.d || true

echo -e "\n--- dpkg audit ---"
dpkg --audit || true

echo -e "\n--- dpkg selections (top 120) ---"
dpkg --get-selections | head -n 120 || true

echo -e "\n--- apt-cache policy summary ---"
apt-cache policy || true

for pkg in libltdl-dev libltdl7 libtool automake tzdata; do
  echo -e "\n--- apt-cache policy ${pkg} ---"
  apt-cache policy "${pkg}" || true
done

echo -e "\n--- apt-get update ---"
apt-get update -qq || true

echo -e "\n--- Simulate install (pkg resolver debug) for libltdl-dev ---"
apt-get -s -o Debug::pkgProblemResolver=yes install libltdl-dev || true

echo -e "\n--- Simulate fix-broken ---"
apt-get -s -o Debug::pkgProblemResolver=yes --fix-broken install || true

echo -e "\n--- Try fix-broken (non-simulated) to report output ---"
apt-get -y --fix-broken install || true
dpkg --configure -a || true

echo -e "\n=== End debug ==="
