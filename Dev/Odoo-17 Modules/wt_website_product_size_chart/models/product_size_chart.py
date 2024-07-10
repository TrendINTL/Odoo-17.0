# -*- coding: utf-8 -*-
# Copyright (C) Wisenetic Technologies.

from odoo import models, fields, api


class ProductSizeChart(models.Model):
    _name = 'website.product.size.chart'
    _description = "Website Product Size Chart"

    name = fields.Char(string='Name', required=True)
    image = fields.Image(string='Image')
    is_active = fields.Boolean(string='Active', required=True, default=True)

    link_text = fields.Char(string='Link Text', required=True)
    row = fields.Integer(string='Row', help='', required=True, default=6)
    column = fields.Integer(string='Column', help='', required=True, default=2)
    template = fields.Html(string='Template', help='',
                           required=True)
    how_to_measure = fields.Html(
        string='How To Measure', help='', required=True)
    product_ids = fields.One2many(
        comodel_name='product.template', inverse_name='size_chart_id', string='Products')
    category_ids = fields.One2many(
        comodel_name='product.category', inverse_name='size_chart_id', string='Product Categories')
