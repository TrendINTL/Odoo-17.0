#!/bin/bash
echo -e "\n
###############################################################################\n
# ########################################################################### #\n
# #                                                                         # #\n
# # ########  #####    ######  ###     ##  #####           #####      ##### # #\n
# #    ##     ##  ##   ##      ## #    ##  ##  ###        ### ###      # #  # #\n
# #    ##     ######   ## ##   ##  #   ##  ##   ###      ###   ###     # #  # #\n
# #    ##     ##  ##   ## ##   ##   #  ##  ##   ###     ###########    # #  # #\n
# #    ##     ##   ##  ##      ##    # ##  ##  ###     ###       ###   # #  # #\n
# #    ##     ##   ##  ######  ##     ###  #####      ####       #### ##### # #\n
# #                                                                         # #\n
# ########################################################################### #\n
###############################################################################
"
# Script for installing Odoo 17.0 on Ubuntu 24.04
# Author: AHMED OUF
#-------------------------------------------------------------------------------
# This script will install Odoo on your Ubuntu server. It can install multiple Odoo instances
# in one Ubuntu because of the different xmlrpc_ports
#-------------------------------------------------------------------------------
# sudo adduser odoo17
# Name: Odoo 17.0 installation User
# sudo usermod -aG sudo odoo17
# sudo su odoo17
# cd
# sudo add-apt-repository ppa:deadsnakes/ppa
# sudo apt-get update
# sudo apt-get upgrade
# sudo apt list | grep python3.10
# sudo apt-get install python3.10
# sudo update-alternatives --install /usr/bin/python3 python3 /usr/bin/python3.8 2
# sudo update-alternatives --install /usr/bin/python3 python3 /usr/bin/python3.10 1
# sudo update-alternatives --config python3
# python3 --version
# pip --version
# sudo cp apt_pkg.cpython-38-x86_64-linux-gnu.so apt_pkg.so
# sudo nano /usr/bin/add-apt-repository
# Change the first line to ==>>> #!/usr/bin/python3.10 
# sudo apt install python3-pip
# sudo apt install python3-pip --reinstall
# Make a new file:
# sudo nano odoo17_install.sh
# Place this content in it and then make the file executable:
# sudo chmod +x odoo17_install.sh
# Execute the script to install Odoo 17.0:
# ./odoo17_install.sh
################################################################################

echo -e "\n #---Install Python3.10 & Update and upgrade the system---#"
sudo apt update
sudo apt upgrade -y

echo -e "\n #---# Install necessary dependencies---#"
sudo apt install -y git build-essential wget python3-dev python3-wheel libxslt-dev libzip-dev libldap2-dev libsasl2-dev python3-setuptools gdebi-core libpq-dev

echo -e "\n #---Install Python 3.10 venv---#"
sudo apt install -y python3.10 python3.10-venv

# Create a Python virtual environment
python3.10 -m venv odoo_env

# Activate the virtual environment
source odoo_env/bin/activate
pip install --upgrade pip

# Handle Jinja2 conflict
pip uninstall -y Jinja2
pip install Jinja2==3.1.2

# Install Python dependencies with resolved versions
pip install \
    Babel==2.9.1 \
    chardet==4.0.0 \
    cryptography==3.4.8 \
    decorator==4.4.2 \
    docutils==0.17 \
    ebaysdk==2.1.5 \
    freezegun==1.1.0 \
    geoip2==2.9.0 \
    gevent==22.10.2 \
    greenlet==2.0.2 \
    idna==2.10 \
    libsass==0.20.1 \
    lxml==5.2.1 \
    MarkupSafe==2.1.2 \
    num2words==0.5.10 \
    ofxparse==0.21 \
    passlib==1.7.4 \
    Pillow==9.4.0 \
    polib==1.1.1 \
    psutil==5.9.4 \
    psycopg \
    pydot==1.4.2 \
    pyopenssl==21.0.0 \
    PyPDF2==4.2.0 \
    pyserial==3.5 \
    python-dateutil==2.8.1 \
    python3-ldap \
    python-stdnum==1.17 \
    pyusb==1.2.1 \
    qrcode==7.3.1 \
    reportlab==3.6.12 \
    requests==2.25.1 \
    rjsmin==1.1.0 \
    urllib3==1.26.5 \
    vobject==0.9.6.1 \
    Werkzeug==2.0.2 \
    xlrd==1.2.0 \
    XlsxWriter==3.0.2 \
    xlwt==1.3.* \
    zeep==4.1.0

# Deactivate the virtual environment
deactivate

# Install PostgreSQL
sudo apt install -y postgresql

# Create PostgreSQL user for Odoo
sudo -u postgres createuser -d -R -S $USER
sudo service postgresql restart

# Install libssl1.1 manually
wget http://archive.ubuntu.com/ubuntu/pool/main/o/openssl/libssl1.1_1.1.1f-1ubuntu2_amd64.deb
sudo dpkg -i libssl1.1_1.1.1f-1ubuntu2_amd64.deb
rm libssl1.1_1.1.1f-1ubuntu2_amd64.deb

# Download and install Wkhtmltopdf
wget https://github.com/wkhtmltopdf/packaging/releases/download/0.12.6-1/wkhtmltox_0.12.6-1.bionic_amd64.deb
sudo apt install -y ./wkhtmltox_0.12.6-1.bionic_amd64.deb
rm wkhtmltox_0.12.6-1.bionic_amd64.deb
sudo apt install -f

# Install Node.js and npm
sudo apt install -y nodejs npm

# Create Odoo directory
sudo mkdir /opt/odoo17
sudo chmod +x /opt/odoo17
sudo chown $USER:$USER /opt/odoo17

# Clone Odoo repository
git clone https://www.github.com/odoo/odoo --branch 17.0 --depth 1 /opt/odoo17/odoo

# Change Odoo Directory Own and Conf:
sudo chmod +x /opt/odoo17/odoo
sudo chown $USER:$USER /opt/odoo17/odoo

# Create Python virtual environment in Odoo directory
python3.10 -m venv /opt/odoo17/odoo/venv

# Activate the virtual environment
source /opt/odoo17/odoo/venv/bin/activate

# Install required Python packages for Odoo
pip install -r /opt/odoo17/odoo/requirements.txt

# Deactivate the virtual environment
deactivate

# Create Addons File
sudo mkdir /opt/odoo17/addons
sudo chmod +x /opt/odoo17/addons
sudo chown $USER:$USER /opt/odoo17/addons

# Create Server Log File
sudo mkdir /var/log/odoo17
sudo chmod +x /var/log/odoo17
sudo chown $USER:$USER /var/log/odoo17

# Create Odoo configuration file
sudo tee /etc/odoo17.conf > /dev/null <<EOF
[options]
; This is the password that allows database operations:
; admin_passwd = 123456
db_host = False
db_port = False
; db_user = odoo17
db_password = False
addons_path = /opt/odoo17/odoo/addons, /opt/odoo17/addons
logfile = /var/log/odoo17/odoo17.log
xmlrpc_port = 8069
EOF

# Change Conf File Own and Conf:
sudo chmod +x /etc/odoo17.conf
sudo chown $USER:$USER /etc/odoo17.conf

# Create systemd service file for Odoo
sudo tee /etc/systemd/system/odoo17.service > /dev/null <<EOF
[Unit]
Description=Odoo 17.0
Documentation=https://www.odoo.com
[Service]
# Ubuntu/Debian convention:
Type=simple
User=$USER
ExecStart=/opt/odoo17/odoo/venv/bin/python3 /opt/odoo17/odoo/odoo-bin -c /etc/odoo17.conf -l /var/log/odoo17/odoo17.log
[Install]
WantedBy=default.target
EOF

# Change Service File Own and Conf:
sudo chmod +x /etc/systemd/system/odoo17.service
sudo chown $USER:$USER /etc/systemd/system/odoo17.service

# Start and enable Odoo service
sudo systemctl daemon-reload
sudo systemctl start odoo17.service
sudo systemctl enable odoo17.service
sudo systemctl restart odoo17.service
sudo service odoo17 restart

# Allow HTTP and HTTPS traffic through the firewall
sudo ufw allow http
sudo ufw allow https

echo "Odoo 17 installation is complete. You can access it at http://your_server_ip:8069"
