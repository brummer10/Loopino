#!/usr/bin/env bash
set -euo pipefail

# robust non-interactive env
export DEBIAN_FRONTEND=noninteractive
echo "DEBIAN_FRONTEND=$DEBIAN_FRONTEND"

echo "=== Date: $(date) : APT / DPKG debugging info ==="

echo -e "\n--- /etc/apt/sources.list.d ---"
ls -la /etc/apt/sources.list.d || true

echo -e "\n--- /etc/apt/sources.list ---"
cat /etc/apt/sources.list || true

echo -e "\n--- apt list --installed (top 80) ---"
dpkg --get-selections | head -n 80 || true

echo -e "\n--- dpkg audit (if half-configured) ---"
dpkg --audit || true

echo -e "\n--- apt-cache policy summary ---"
apt-cache policy || true

# show policy for likely problematic packages
for pkg in libltdl-dev libltdl7 libtool automake; do
  echo -e "\n--- apt-cache policy ${pkg} ---"
  apt-cache policy "${pkg}" || true
done

echo -e "\n--- apt-get update ---"
apt-get update -qq || true

echo -e "\n--- Simulate install with problem-resolver debug for libltdl-dev ---"
apt-get -s -o Debug::pkgProblemResolver=yes install libltdl-dev || true

echo -e "\n--- Try fix-broken in simulation ---"
apt-get -s -o Debug::pkgProblemResolver=yes --fix-broken install || true

echo -e "\n--- dpkg --configure -a (dry-run show) ---"
dpkg --configure -a || true

echo -e "\n=== End debug ==="