# -*- coding: utf-8 -*-
# Part of BrowseInfo. See LICENSE file for full copyright and licensing details.

{
    "name" : "Website Customer Social Network",
    "version" : "16.0.0.2",
    "category" : "Extra Tools",
    'license':'OPL-1',
    'summary': 'Website Customer/Partner Social Network',
    "description": """
        This Module add social networks detail on Partner and shows them on website
    """,
    "author": "BrowseInfo",
    "website" : "https://www.browseinfo.com",
    "depends" : ['base','sale','website','website_crm_partner_assign','website_partner'],
    "data" :[
			'views/partner_social_network.xml',
            'views/template.xml',
            ],
    'qweb':[],
    "auto_install": False,
    "installable": True,
    "live_test_url":'https://youtu.be/LTCaW5UDR8k',
    "images":['static/description/Banner.gif'],
}
# vim:expandtab:smartindent:tabstop=4:softtabstop=4:shiftwidth=4:
