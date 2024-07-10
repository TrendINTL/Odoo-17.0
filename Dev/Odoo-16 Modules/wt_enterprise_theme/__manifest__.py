{
    'name': 'WT Odoo Enterprise Theme',
    'version': '16.0.1.0.1',
    'summary': 'WT Odoo Enterprise Theme',
    'author': 'Waqas Riasat',
    'license': 'AGPL-3',
    'maintainer': 'Waqas Riasat',
    'company': 'Waqas Riasat',
    'website': 'https://way4tech.cloud',
    'depends': [
        'web'
    ],
    'category':'App',
    'description': """
           Way4tech Odoo Enterprise Theme
    """,
    'assets': {
        'web._assets_primary_variables': [
            '/wt_enterprise_theme/static/src/scss/primary_variables_custom.scss',
        ],
        'web.assets_backend': [
            '/wt_enterprise_theme/static/src/scss/wt_custom.scss',
        ],
        'web.assets_common': [
            '/wt_enterprise_theme/static/src/scss/wt_custom.scss',
        ],
        'web._assets_secondary_variables': [
            '/wt_enterprise_theme/static/src/scss/secondary_variables.scss',
        ],
    },
    'price':0,
    'currency':'USD',
    'installable': True,
    'application': True,
    'images': ['static/description/Screenshot_4.jpg']
}
