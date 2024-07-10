# -*- encoding: utf-8 -*-
{
    'name': "Mass Mailing",
    'version': '1.0',
    'summary': 'Comprehensive solutions to email marketing and related problems.',
    'category': 'Mail',
    'description': """
        A product of Bac Ha Software provides comprehensive solutions to email marketing and related problems.
    """,
    "depends": ['mass_mailing', 'base'],
    'external_dependencies': {
        'python': ['boto3'],
    },
    'data': [
        'security/ir.model.access.csv',
        'data/mailing_contact_data.xml',
        'views/mailing_mailing_views.xml',
        'views/mailing_blacklist_domain_views.xml',
        'views/mailing_contacted_views.xml',
        'wizards/mailing_contact_to_list_views.xml',
    ],
    # Author
    'author': 'Bac Ha Software',
    'website': 'https://bachasoftware.com',
    'maintainer': 'Bac Ha Software',
    'images': ['static/description/banner.gif'],
    'installable': True,
    'application': True,
    'auto_install': False,
    'license': 'LGPL-3'
}
