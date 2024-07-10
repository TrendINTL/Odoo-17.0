# -*- coding: utf-8 -*-

import boto3
from datetime import timedelta
from odoo import models, fields, api, _
from odoo.exceptions import ValidationError
from odoo.tools.safe_eval import safe_eval


class MassMailing(models.Model):

    _inherit = 'mailing.mailing'

    schedule_date = fields.Datetime(copy=False)

    # def _compute_next_departure(self):
    #     cron_next_call = self.env.ref('mass_mailing.ir_cron_mass_mailing_queue').sudo().nextcall
    #     str2dt = fields.Datetime.from_string
    #     cron_time = str2dt(cron_next_call)
    #     for mass_mailing in self:
    #         if mass_mailing.schedule_date or mass_mailing.schedule_type == 'now':
    #             schedule_date = str2dt(mass_mailing.schedule_date or fields.Datetime.now())
    #             mass_mailing.next_departure = min(schedule_date, cron_time)
    #         else:
    #             mass_mailing.next_departure = cron_time
    #
    # def write(self, vals):
    #     res = super(MassMailing, self).write(vals)
    #     if vals.get('schedule_date'):
    #         mailing_cron = self.sudo().env.ref('mass_mailing.ir_cron_mass_mailing_queue')
    #         schedule_date = fields.Datetime.from_string(vals['schedule_date']) if isinstance(vals['schedule_date'], str) else vals['schedule_date']
    #         if mailing_cron.nextcall < schedule_date:
    #             mailing_cron.write({
    #                 'nextcall': schedule_date + timedelta(hours=6),
    #             })
    #     return res


class MailingContact(models.Model):

    _inherit = 'mailing.contact'

    def clean_blacklist_email_in_contact_lists(self):
        odoo_blacklist = self.env['mailing.contact'].sudo().search([('is_blacklisted', '=', True)])
        odoo_blacklist.unlink()

        return {
            'type': 'ir.actions.client',
            'tag': 'display_notification',
            'params': {
                'type': 'success',
                'message': _('Clean all blacklist email in contact lists!'),
                'next': {'type': 'ir.actions.act_window_close'},
            }
        }

    def write(self, vals):
        # print(vals.get('email'))
        return super(MailingContact, self).write(vals)

    def check_blacklist_domain(self, email, blacklist_domain):
        match = email.split("@")
        if len(match) > 1:
            for domain in blacklist_domain:
                if match[1].find(domain) >= 0:
                    return True
            return False

    @api.model_create_multi
    def create(self, vals_list):
        if safe_eval(self.env['ir.config_parameter'].sudo().get_param('mailing_contact.email_unique')):
            emails = [vals['email'] for vals in vals_list if vals.get('email')]
            existing_contacts = self.search([('email', 'in', emails)])

            existing_contacted = self.env['mailing.contact.contacted'].search([('email', 'in', emails)])
            existing_emails = existing_contacts.mapped('email')
            update_lists_mode = self.env.context.get('update_lists_mode')
            existing_emails_contacted = existing_contacted.mapped('email') if not update_lists_mode else []

            blacklist_domains = self.env['mailing.blacklist.domain'].sudo().search([])
            blacklist_domain_vals = blacklist_domains.mapped('name')

            # update existing contacts to the current mailing list
            # if self.env.context.get('default_list_ids') or (self.env.context.get('active_model') == 'mailing.list' and self.env.context.get('active_ids')):
            #     MailingSubscription = self.env['mailing.contact.subscription']
            #     list_ids = self.env.context.get('default_list_ids') or self.env.context.get('active_ids')
            #     sub_vals_list = []
            #     for list_id in list_ids:
            #         # avoid existing subscriptions
            #         existing_sub_contacts = MailingSubscription.sudo().search([('list_id', '=', list_id), ('contact_id', 'in', existing_contacts.ids)]).mapped('contact_id')
            #         sub_vals_list += [{'contact_id': contact_id, 'list_id': list_id} for contact_id in existing_contacts.ids if contact_id not in existing_sub_contacts.ids]
            #     MailingSubscription.create(sub_vals_list)

            vals_list_add = [vals for vals in vals_list if vals.get('email') not in existing_emails]
            vals_add_blacklist = [vals for vals in vals_list if self.check_blacklist_domain(vals.get('email'), blacklist_domain_vals)]

            vals_list_write = [vals for vals in vals_list if vals.get('list_ids') and vals.get('email') in existing_emails and vals.get('email') not in existing_emails_contacted]
            vals_list_write_list_ids = [vals for vals in vals_list if vals.get('email') in existing_emails and vals.get('email') not in existing_emails_contacted]

            for vals_contact in vals_add_blacklist:
                self.env['mail.blacklist'].sudo()._add(vals_contact.get('email'))

            # Update mailing list of contact
            for vals in vals_list_write:
                self.search([('email', '=', vals.get('email'))]).write({
                    'list_ids': vals.get('list_ids')
                })
            # Update mailing list of contact without column 'list_ids' in upload file
            if self.env.context.get('default_mailing_list_ids') or self.env.context.get('from_mailing_list_ids'):
                for vals in vals_list_write_list_ids:
                    self.search([('email', '=', vals.get('email'))]).write({
                        'list_ids': self.env.context.get('default_mailing_list_ids') or self.env.context.get('from_mailing_list_ids')
                    })
            elif self.env.context.get('active_model') == 'mailing.list' and self.env.context.get('active_ids'):
                for vals in vals_list_write_list_ids:
                    self.search([('email', '=', vals.get('email'))]).write({
                        'list_ids': self.env.context.get('active_ids')
                    })

            blacklist_contacts = self.env['mail.blacklist'].sudo().search([('email', 'in', emails)])
            blacklist_emails = blacklist_contacts.mapped('email')
            vals_blacklist = [vals.get('email') for vals in vals_list if vals.get('email') in blacklist_emails]

            retval = existing_contacts + super(MailingContact, self).create(vals_list_add)
            # xoa cac contact là blacklist email
            self.remove_blacklist_contacts(vals_blacklist)

            return retval

        return super(MailingContact, self).create(vals_list)

    @api.constrains('email')
    def _check_email(self):
        if safe_eval(self.env['ir.config_parameter'].sudo().get_param('mailing_contact.email_unique')):
            for contact in self.filtered(lambda c: c.email):
                if self.search([('email', '=', contact.email), ('id', '!=', contact.id)]):
                    raise ValidationError(_('Email must be unique.'))

    @api.model
    def delete_duplicate_contacts(self):
        if safe_eval(self.env['ir.config_parameter'].sudo().get_param('mailing_contact.email_unique')):
            emails_res = self.read_group([('email', '!=', False), ('email', '!=', '')], ['email'], ['email'], lazy=False)
            for email_res in emails_res:
                if email_res['__count'] > 1:
                    contacts = self.search([('email', '=', email_res['email'])], order='id asc')
                    contacts[1:].unlink()

    @api.model
    def remove_blacklist_contacts(self, blacklist=[]):
        if blacklist:
            suppressed_mail_contact = self.sudo().search([('email', 'in', blacklist)])
            if suppressed_mail_contact:
                suppressed_mail_contact.unlink()

    @api.model
    def _delete_suppressed(self, days=21):
        Config = self.env['ir.config_parameter'].sudo()
        access_key = Config.get_param('ses_user_access_key')
        secret_key = Config.get_param('ses_user_secret_key')
        region = Config.get_param('ses_region')
        if access_key and secret_key and region:
            end_date = fields.Datetime.now()
            start_date = end_date - timedelta(days=days)
            client = boto3.client('sesv2',
                                  aws_access_key_id=access_key,
                                  aws_secret_access_key=secret_key,
                                  region_name=region)
            response = client.list_suppressed_destinations(
                Reasons=['BOUNCE', 'COMPLAINT'],
                StartDate=start_date,
                EndDate=end_date,
            )
            if response.get('SuppressedDestinationSummaries'):
                suppressed_email = [res['EmailAddress'] for res in response.get('SuppressedDestinationSummaries')]
                # thêm vào blacklist
                for suppressed_mail_contact in suppressed_email:
                    self.env['mail.blacklist'].sudo()._add(suppressed_mail_contact)

                # xoa cac contact là blacklist moi duoc them vao danh sach
                self.remove_blacklist_contacts(suppressed_email)

                # suppressed_mail_contact = self.sudo().search([('email', 'in', suppressed_email)])
                # if suppressed_mail_contact:
                #     suppressed_mail_contact.unlink()

    def add_to_blacklist(self):
        for contact in self:
            if contact.email:
                self.env['mail.blacklist'].sudo()._add(contact.email)


class BlacklistMailingDomain(models.Model):
    _name = 'mailing.blacklist.domain'
    _description = 'Mailing blacklist domain'

    name = fields.Char('Domain')

