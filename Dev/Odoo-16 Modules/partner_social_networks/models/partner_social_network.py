# -*- coding: utf-8 -*-
# Part of BrowseInfo. See LICENSE file for full copyright and licensing details.

from odoo import models, fields, api, _
import odoo.addons.decimal_precision as dp
import time


class res_partner(models.Model):
    _inherit = "res.partner"
    
    facebook_url = fields.Char('Facebook')
    twitter_url = fields.Char('Twitter')
    youtube_url = fields.Char('Youtube')
    instagram_url = fields.Char('instagram')
    googleplus_url = fields.Char('Google+')

    _defaults = {
    }
