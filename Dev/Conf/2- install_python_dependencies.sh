#!/bin/bash

# Update package lists
sudo apt update

# Install system dependencies
sudo apt install -y python3 python3-pip python3-venv

# Create a Python virtual environment
python3 -m venv odoo_env

# Activate the virtual environment
source odoo_env/bin/activate

# Determine Python version
python_version=$(python3 -c "import sys; print('.'.join(map(str, sys.version_info[:2])))")

# Install Python dependencies with conditional versions
if [[ $python_version == "3.10" ]]; then
    jinja_version="Jinja2==3.1.2"
    greenlet_version="greenlet==1.1.2"
    lxml_version="lxml==4.8.0"
    pillow_version="Pillow==9.0.1"
    psutil_version="psutil==5.9.0"
    pypdf2_version="PyPDF2==1.26.0"
    reportlab_version="reportlab==3.6.8"
elif [[ $python_version > "3.10" ]]; then
    jinja_version="Jinja2==3.1.2"
    greenlet_version="greenlet==2.0.2"
    lxml_version="lxml==4.9.2"
    pillow_version="Pillow==9.4.0"
    psutil_version="psutil==5.9.4"
    pypdf2_version="PyPDF2==2.12.1"
    reportlab_version="reportlab==3.6.12"
else
    jinja_version="Jinja2==3.0.3"
    greenlet_version="greenlet==1.1.2"
    lxml_version="lxml==4.8.0"
    pillow_version="Pillow==9.0.1"
    psutil_version="psutil==5.9.0"
    pypdf2_version="PyPDF2==1.26.0"
    reportlab_version="reportlab==3.6.8"
fi

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
    $greenlet_version \
    idna==2.10 \
    $jinja_version \
    libsass==0.20.1 \
    $lxml_version \
    MarkupSafe==2.0.1 \
    num2words==0.5.10 \
    ofxparse==0.21 \
    passlib==1.7.4 \
    $pillow_version \
    polib==1.1.1 \
    $psutil_version \
    psycopg2==2.9.2 \
    pydot==1.4.2 \
    pyopenssl==21.0.0 \
    $pypdf2_version \
    python-dateutil==2.8.1 \
    python-ldap==3.4.0 \
    python-stdnum==1.17 \
    pyusb==1.2.1 \
    qrcode==7.3.1 \
    $reportlab_version \
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