#!/bin/bash

# Update package lists
sudo apt update

# Install Python 3.10
sudo apt install -y python3.10 python3.10-venv

# Create a Python virtual environment
python3.10 -m venv odoo_env

# Activate the virtual environment
source odoo_env/bin/activate

# Install Python dependencies
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
    Jinja2==3.1.2 \
    libsass==0.20.1 \
    lxml==5.2.1 \
    MarkupSafe==2.1.2 \
    num2words==0.5.10 \
    ofxparse==0.21 \
    passlib==1.7.4 \
    Pillow==9.4.0 \
    polib==1.1.1 \
    psutil==5.9.4 \
    psycopg2==2.9.5 \
    pydot==1.4.2 \
    pyopenssl==21.0.0 \
    PyPDF2==2.12.1 \
    pyserial==3.5 \
    python-dateutil==2.8.1 \
    python-ldap==3.4.0 \
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

echo "Python dependencies installation complete."