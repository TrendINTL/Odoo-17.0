#!/bin/bash
echo -e "\n
###############################################################################\n
# ############################################################################ #\n
# #                                                                          # #\n
# # ########  #####    ######  ###     ##  #####            #####      ##### # #\n
# #    ##     ##  ##   ##      ## #    ##  ##  ###         ### ###      # #  # #\n
# #    ##     ######   ## ##   ##  #   ##  ##   ###       ###   ###     # #  # #\n
# #    ##     ##  ##   ## ##   ##   #  ##  ##   ###      ###########    # #  # #\n
# #    ##     ##   ##  ##      ##    # ##  ##  ###      ###       ###   # #  # #\n
# #    ##     ##   ##  ######  ##     ###  #####       ####       #### ##### # #\n
# #                                                                          # #\n
# ############################################################################ #\n
################################################################################
"
# Script for installing Odoo 17.0 on Ubuntu 24.04
# Author: AHMED OUF
#-------------------------------------------------------------------------------
# This script will install Odoo on your Ubuntu server. It can install multiple Odoo instances
# in one Ubuntu because of the different xmlrpc_ports
#-------------------------------------------------------------------------------
# sudo adduser alus
# Name: Odoo 17.0 ALUS installation User
# sudo usermod -aG sudo alus
# sudo su alus
# cd
# sudo nano alus_install.sh
# Place this content in it and then make the file executable:
# sudo chmod +x alus_install.sh
# Execute the script to install Odoo 17.0:
# ./alus_install.sh
################################################################################

# Create PostgreSQL user for Odoo
sudo -u postgres createuser -d -R -S $USER
sudo service postgresql restart

# Create Odoo directory
sudo mkdir /opt/odoo17/alus
sudo chmod +x /opt/odoo17/alus
sudo chown $USER:$USER /opt/odoo17/alus

# Clone Odoo repository
git clone https://www.github.com/odoo/odoo --branch 17.0 --depth 1 /opt/odoo17/alus/odoo

# Change Odoo Directory Own and Conf:
sudo chmod +x /opt/odoo17/alus/odoo
sudo chown $USER:$USER /opt/odoo17/alus/odoo

# Create Python virtual environment in Odoo directory
python3.10 -m venv /opt/odoo17/alus/odoo/venv

# Activate the virtual environment
source /opt/odoo17/alus/odoo/venv/bin/activate

# Install required Python packages for Odoo
# pip install -r /opt/odoo17/alus/odoo/requirements.txt

sudo apt install python3-pip
sudo apt install python3-pip --reinstall
pip install --upgrade pip

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
    lxml_html_clean \
    Jinja2 \
    MarkupSafe==2.1.2 \
    num2words==0.5.10 \
    ofxparse==0.21 \
    passlib==1.7.4 \
    Pillow==9.4.0 \
    polib==1.1.1 \
    psutil==5.9.4 \
    psycopg2-binary \
    pydot==1.4.2 \
    pyopenssl==21.0.0 \
    PyPDF2 \
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

# Create Odoo configuration file
sudo tee /etc/alus.conf > /dev/null <<EOF
[options]
; This is the password that allows database operations:
; admin_passwd = 123456
db_host = False
db_port = False
; db_user = alus
db_password = False
addons_path = /opt/odoo17/alus/odoo/addons, /opt/odoo17/addons
logfile = /var/log/odoo17/alus.log
xmlrpc_port = 8087
EOF

# Change Conf File Own and Conf:
sudo chmod +x /etc/alus.conf
sudo chown $USER:$USER /etc/alus.conf

# Create systemd service file for Odoo
sudo tee /etc/systemd/system/alus.service > /dev/null <<EOF
[Unit]
Description=Odoo 17.0
Documentation=https://www.odoo.com
[Service]
# Ubuntu/Debian convention:
Type=simple
User=$USER
ExecStart=/opt/odoo17/alus/odoo/venv/bin/python3 /opt/odoo17/alus/odoo/odoo-bin -c /etc/alus.conf -l /var/log/odoo17/alus.log
[Install]
WantedBy=default.target
EOF

# Change Service File Own and Conf:
sudo chmod +x /etc/systemd/system/alus.service
sudo chown $USER:$USER /etc/systemd/system/alus.service

# Start and enable Odoo service
sudo systemctl daemon-reload
sudo systemctl start alus.service
sudo systemctl enable alus.service
sudo systemctl restart alus.service
sudo service alus restart

# Allow HTTP and HTTPS traffic through the firewall
#sudo ufw allow http
#sudo ufw allow https

echo "Odoo 17 installation for ALUS is complete. You can access it at http://your_server_ip:8087"