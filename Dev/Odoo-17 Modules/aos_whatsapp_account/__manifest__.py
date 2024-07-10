# See LICENSE file for full copyright and licensing details.

{
    'name': 'Whatsapp Account',
    'version': '17.0.0.1.0',
    'license': 'OPL-1',
    'author': "Alphasoft",
    'sequence': 1,
    'website': 'https://www.alphasoft.co.id/',
    'images':  ['images/main_screenshot.png'],
    'summary': 'This module is used for Whatsapp Account Send Multi Invoice',
    'category': 'Extra Tools',
    'depends': ['account', 'aos_whatsapp'],
    'data': [
        'wizard/account_move_send_views.xml',
    ],
    'external_dependencies': {'python': ['html2text']},
    'demo': [],
    'test': [],
    'css': [],
    'js': [],
    'price': 0,
    'currency': 'EUR',
    'installable': True,
    'application': False,
    'auto_install': False,
}