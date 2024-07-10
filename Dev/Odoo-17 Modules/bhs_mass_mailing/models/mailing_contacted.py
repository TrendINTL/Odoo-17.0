from odoo import api, fields, models, tools, _
from odoo.exceptions import UserError


class BHMailingContacted(models.Model):
    _name = "mailing.contact.contacted"
    _description = "Mailing contact contacted"

    email = fields.Char(string='Email Address')

    _sql_constraints = [
        ('unique_email', 'unique (email)', 'Email address already exists!')
    ]

    @api.model_create_multi
    def create(self, values):
        # First of all, extract values to ensure emails are really unique (and don't modify values in place)
        new_values = []
        all_emails = []
        for value in values:
            email = tools.email_normalize(value.get('email'))
            if not email:
                raise UserError(_('Invalid email address %r', value['email']))
            if email in all_emails:
                continue
            all_emails.append(email)
            new_value = dict(value, email=email)
            new_values.append(new_value)

        """ To avoid crash during import due to unique email, return the existing records if any """
        to_create = []
        bl_entries = {}
        if new_values:
            sql = '''SELECT email, id FROM mailing_contact_contacted WHERE email = ANY(%s)'''
            emails = [v['email'] for v in new_values]
            self._cr.execute(sql, (emails,))
            bl_entries = dict(self._cr.fetchall())
            to_create = [v for v in new_values if v['email'] not in bl_entries]

        # TODO DBE Fixme : reorder ids according to incoming ids.
        results = super(BHMailingContacted, self).create(to_create)
        return self.env['mailing.contact.contacted'].browse(bl_entries.values()) | results