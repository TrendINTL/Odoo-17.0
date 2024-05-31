#!/bin/bash

###############################################################################
# ########################################################################### #
# #							  		    # #
# # ########  #####    ######  ###     ##  #####           #####      ##### # #
# #    ##     ##  ##   ##      ## #    ##  ##  ###        ### ###      # #  # #
# #    ##     ######   ## ##   ##  #   ##  ##   ###      ###   ###     # #  # #
# #    ##     ##  ##   ## ##   ##   #  ##  ##   ###     ###########    # #  # #
# #    ##     ##   ##  ##      ##    # ##  ##  ###     ###       ###   # #  # #
# #    ##     ##   ##  ######  ##     ###  #####      ####       #### ##### # #
# #                                                                         # #
# ########################################################################### #
###############################################################################
# Script for installing Odoo 17.0 on Ubuntu 24.04
# Author: AHMED OUF
#-------------------------------------------------------------------------------
# This script will install Odoo on your Ubuntu server. It can install multiple Odoo instances
# in one Ubuntu because of the different xmlrpc_ports
#-------------------------------------------------------------------------------
# Make a new file:
# sudo nano odoo17_install.sh
# Place this content in it and then make the file executable:
# sudo chmod +x odoo17_install.sh
# Execute the script to install Odoo 17.0:
# ./odoo17_install.sh
################################################################################

# Update and upgrade the system
sudo apt update
sudo apt upgrade -y

# Install necessary dependencies
sudo apt install -y git python3-pip build-essential wget python3-dev python3-venv \
  python3-wheel libxslt-dev libzip-dev libldap2-dev libsasl2-dev python3-setuptools \
  node-less gdebi-core libpq-dev

# Install PostgreSQL
sudo apt install -y postgresql

# Create PostgreSQL user for Odoo
sudo -u postgres psql -c "CREATE USER odoo17 WITH CREATEDB PASSWORD '123456';"

# Install libssl1.1 manually
wget http://archive.ubuntu.com/ubuntu/pool/main/o/openssl/libssl1.1_1.1.1f-1ubuntu2_amd64.deb
sudo dpkg -i libssl1.1_1.1.1f-1ubuntu2_amd64.deb
rm libssl1.1_1.1.1f-1ubuntu2_amd64.deb

# Download and install Wkhtmltopdf
wget https://github.com/wkhtmltopdf/packaging/releases/download/0.12.6-1/wkhtmltox_0.12.6-1.bionic_amd64.deb
sudo apt install -y ./wkhtmltox_0.12.6-1.bionic_amd64.deb
rm wkhtmltox_0.12.6-1.bionic_amd64.deb

# Create Odoo directory
sudo mkdir /opt/odoo17
sudo chown $USER:$USER /opt/odoo17

# Clone Odoo repository
git clone https://www.github.com/odoo/odoo --branch 17.0 --depth 1 /opt/odoo17/odoo

# Create Python virtual environment and install dependencies
cd /opt/odoo17/odoo
python3 -m venv venv
source venv/bin/activate
pip install -r requirements.txt
deactivate

# Create Odoo configuration file
sudo tee /etc/odoo17.conf > /dev/null <<EOF
[options]
   ; This is the password that allows database operations:
   admin_passwd = admin
   db_host = False
   db_port = False
   db_user = odoo17
   db_password = 123456
   addons_path = /opt/odoo17/odoo/addons
   logfile = /var/log/odoo17/odoo.log
EOF

# Create systemd service file for Odoo
sudo tee /etc/systemd/system/odoo17.service > /dev/null <<EOF
[Unit]
Description=Odoo
Documentation=https://www.odoo.com
[Service]
# Ubuntu/Debian convention:
Type=simple
User=$USER
ExecStart=/opt/odoo17/odoo/venv/bin/python3 /opt/odoo17/odoo/odoo-bin -c /etc/odoo17.conf
[Install]
WantedBy=default.target
EOF

# Start and enable Odoo service
sudo systemctl daemon-reload
sudo systemctl start odoo17
sudo systemctl enable odoo17

# Allow HTTP and HTTPS traffic through the firewall
sudo ufw allow http
sudo ufw allow https

echo "Odoo 17 installation is complete. You can access it at http://your_server_ip:8069"
