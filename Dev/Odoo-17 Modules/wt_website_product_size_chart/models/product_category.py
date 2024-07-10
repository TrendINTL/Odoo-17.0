# -*- coding: utf-8 -*-
# Copyright (C) Wisenetic Technologies.
from odoo import models, fields, api
from odoo.addons.l10n_ar_website_sale.models import website


class ProductCategory(models.Model):
    _inherit = 'product.category'

    size_chart_id = fields.Many2one(
        'website.product.size.chart', string='Size Chart')
