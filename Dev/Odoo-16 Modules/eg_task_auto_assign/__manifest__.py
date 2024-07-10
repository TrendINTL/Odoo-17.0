{
    'name': 'Auto Assign Task',
    'version': '16.0.1.0.0',
    'category': 'Services/Project',
    'summary': 'Auto Assign Task',
    'author': 'INKERP',
    'website': 'https://www.inkerp.com/',
    'depends': ['project'],
    
    'data': [
        'security/ir.model.access.csv',
        'views/project_project_view.xml',
    ],
    
    'images': ['static/description/banner.png'],
    'license': "OPL-1",
    'installable': True,
    'application': True,
    'auto_install': False,
}
