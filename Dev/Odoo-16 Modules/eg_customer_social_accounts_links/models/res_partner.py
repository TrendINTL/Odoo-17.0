from odoo import models, fields

class ResPartner(models.Model):
    _inherit = 'res.partner'

    facebook_link = fields.Char(string='Facebook')
    instagram_link = fields.Char(string='Instagram')
    pinterest_link = fields.Char(string='Pinterest')
    twitter_link = fields.Char(string='Twitter')
    linkedin_link = fields.Char(string='Linkedin')
    youtube_link = fields.Char(string='Youtube')


