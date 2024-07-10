from odoo import models, fields, api, _
from odoo.exceptions import ValidationError


class SaleOrder(models.Model):
    _inherit = 'sale.order'

    person_signature = fields.Binary(string='signature', inverse='_check_person_signature')
    notes = fields.Char(string="Note")
    signature_required = fields.Boolean(string='signature for sale order',related="company_id.signature_required")
    # sign_user_id = fields.Many2one('res.users', string="User", store=True)


    def _check_person_signature(self):
        for order in self:
            if order.signature_required:
                if not order.person_signature:
                    if order.state == 'draft':
                        raise ValidationError("Please sign the document to proceed.")







class reconfigsetting(models.TransientModel):
    _inherit = 'res.config.settings'

    signature_required = fields.Boolean('signature for sale order',readonly=False,related="company_id.signature_required")


class ResCompany(models.Model):
    _inherit = 'res.company' 

    signature_required = fields.Boolean('signature for sale order')   


