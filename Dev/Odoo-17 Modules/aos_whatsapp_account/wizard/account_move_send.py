# -*- coding: utf-8 -*-
import html2text
import base64
from markupsafe import Markup
from werkzeug.urls import url_encode

from odoo.addons.aos_whatsapp.klikapi import texttohtml
from odoo.tools.mimetypes import guess_mimetype
from odoo import _, api, fields, models, modules, tools, Command
from odoo.exceptions import UserError
from odoo.tools.misc import get_lang


class AccountMoveSend(models.TransientModel):
    _inherit = 'account.move.send'

    def format_amount(self, amount, currency):
        fmt = "%.{0}f".format(currency.decimal_places)
        lang = self.env['res.lang']._lang_get(self.env.context.get('lang') or 'en_US')
 
        formatted_amount = lang.format(fmt, currency.round(amount), grouping=True, monetary=True)\
            .replace(r' ', u'\N{NO-BREAK SPACE}').replace(r'-', u'-\N{ZERO WIDTH NO-BREAK SPACE}')
 
        pre = post = u''
        if currency.position == 'before':
            pre = u'{symbol}\N{NO-BREAK SPACE}'.format(symbol=currency.symbol or '')
        else:
            post = u'\N{NO-BREAK SPACE}{symbol}'.format(symbol=currency.symbol or '')
 
        return u'{pre}{0}{post}'.format(formatted_amount, pre=pre, post=post)

    # == WHATSAPP ==
    enable_send_whatsapp = fields.Boolean(compute='_compute_enable_send_whatsapp')
    checkbox_send_whatsapp = fields.Boolean(
        string="Whatsapp",
        compute='_compute_checkbox_send_whatsapp',
        store=True,
        readonly=False,
    )
    def _get_wizard_values(self):
        res = super(AccountMoveSend, self)._get_wizard_values()
        res['send_whatsapp'] = self.checkbox_send_whatsapp
        return res

    @api.depends('move_ids')
    def _compute_enable_send_whatsapp(self):
        for wizard in self:
            wizard.enable_send_whatsapp = wizard.mode in ('invoice_single', 'invoice_multi')

    @api.depends('enable_send_whatsapp')
    def _compute_checkbox_send_whatsapp(self):
        for wizard in self:
            wizard.checkbox_send_whatsapp = wizard.company_id.invoice_is_email and not wizard.send_mail_readonly
    
    @api.model
    def _send_whatsapp(self, move, mail_template, attachment_id, **kwargs):
        """ Send the journal entry passed as parameter by whatsapp. """
        partner_ids = kwargs.get('partner_ids', [])
        new_message = move\
            .with_context(
                no_new_invoice=True,
                mail_notify_author=self.env.user.partner_id.id in partner_ids,
            ).message_post(
                message_type='whatsapp',
                **kwargs,
                **{
                    'email_layout_xmlid': 'mail.mail_notification_layout_with_responsible_signature',
                    'email_add_signature': not mail_template,
                    'mail_auto_delete': mail_template.auto_delete,
                    'mail_server_id': mail_template.mail_server_id.id,
                    'reply_to_force_new': False,
                },
            )
        
        # 'body': att['datas'].split(",")[0],
        # 'filename': att['filename'],
        mimetype = guess_mimetype(base64.b64decode(attachment_id.datas))
        if mimetype == 'application/octet-stream':
            mimetype = 'video/mp4'
        str_mimetype = 'data:' + mimetype + ';base64,'
        attachment_datas = str_mimetype + str(attachment_id.datas.decode("utf-8"))
        # print ('==kwargs==',attachment_id.name,attachment_id.datas[:15],attachment_datas[:15])
        partner = move.partner_id
        whatsapp = partner._formatting_mobile_number()
        origin = move.name
        amount_total = move.amount_total
        currency_id = move.currency_id
        link = ''
        status = 'pending'
        # composer.message = html2text.html2text(self.template_id.body_html) if self.template_id.body_html else 'Sample Message'
        body_text = html2text.html2text(new_message.body.replace('_PARTNER_', partner.name).replace('_NUMBER_', origin).replace('_AMOUNT_TOTAL_', str(self.format_amount(amount_total, currency_id)) if currency_id else '').replace('\xa0', ' '))
        message_data = {
            'method': 'sendMessage',
            'phone': whatsapp,
            'chatId': partner.chat_id or '',
            'body': body_text,
            'origin': origin,
            'link': link,
        }
        if partner.whatsapp == '0' and partner.chat_id:
            message_data.update({'phone': '','chatId': partner.chat_id})
        if attachment_datas:
            message_data.update({
                'method': 'sendFile',
                'phone': whatsapp,
                'chatId': partner.chat_id or '',
                'body': body_text,
                'filename': origin.replace(' ','_').replace('-','_'),
                'caption': body_text,
                'origin': origin,
                'link': link,
            })
        
        new_message.whatsapp_data = message_data
        new_message.whatsapp_method = message_data['method']
        new_message.whatsapp_chat_id = partner.chat_id
        new_message.whatsapp_status = status
        # Prevent duplicated attachments linked to the invoice.
        new_message.attachment_ids.write({
            'res_model': new_message._name,
            'res_id': new_message.id,
        })

    @api.model
    def _get_whatsapp_params(self, move, move_data):
        # We must ensure the newly created PDF are added. At this point, the PDF has been generated but not added
        # to 'mail_attachments_widget'.
        mail_attachments_widget = move_data.get('mail_attachments_widget')
        seen_attachment_ids = set()
        to_exclude = {x['name'] for x in mail_attachments_widget if x.get('skip')}
        for attachment_data in self._get_invoice_extra_attachments_data(move) + mail_attachments_widget:
            if attachment_data['name'] in to_exclude:
                continue

            try:
                attachment_id = int(attachment_data['id'])
            except ValueError:
                continue

            seen_attachment_ids.add(attachment_id)

        mail_attachments = [
            (attachment.name, attachment.raw)
            for attachment in self.env['ir.attachment'].browse(list(seen_attachment_ids)).exists()
        ]

        return {
            'body': move_data['mail_body'],
            'subject': move_data['mail_subject'],
            'partner_ids': move_data['mail_partner_ids'].ids,
            'attachments': mail_attachments,
        }, self.env['ir.attachment'].browse(list(seen_attachment_ids)).exists()

    @api.model
    def _send_whatsapps(self, moves_data):
        subtype = self.env.ref('mail.mt_comment')
        for move, move_data in [(move, move_data) for move, move_data in moves_data.items() if move.partner_id.whatsapp]:
            mail_template = move_data['mail_template_id']
            mail_lang = move_data['mail_lang']
            mail_params, attachment_id = self._get_whatsapp_params(move, move_data)
            if not mail_params:
                continue

            if move_data.get('proforma_pdf_attachment'):
                attachment = move_data['proforma_pdf_attachment']
                mail_params['attachments'].append((attachment.name, attachment.raw))

            email_from = self._get_mail_default_field_value_from_template(mail_template, mail_lang, move, 'email_from')
            model_description = move.with_context(lang=mail_lang).type_name

            self._send_whatsapp(
                move,
                mail_template,
                attachment_id,
                subtype_id=subtype.id,
                model_description=model_description,
                email_from=email_from,
                **mail_params,
            )

    @api.model
    def _hook_if_whatsapp_success(self, moves_data, from_cron=False, allow_fallback_pdf=False):
        """ Process successful documents.
        :param from_cron:   Flag indicating if the method is called from a cron. In that case, we avoid raising any
                            error.
        :param allow_fallback_pdf:  In case of error when generating the documents for invoices, generate a
                                    proforma PDF report instead.
        """
        to_send_whatsapp = {move: move_data for move, move_data in moves_data.items() if move_data.get('send_whatsapp')}
        self._send_whatsapps(to_send_whatsapp)

    @api.model
    def _send_whatsapp(self, move, mail_template, attachment_id, **kwargs):
        """ Send the journal entry passed as parameter by whatsapp. """
        partner_ids = kwargs.get('partner_ids', [])
        new_message = move\
            .with_context(
                no_new_invoice=True,
                mail_notify_author=self.env.user.partner_id.id in partner_ids,
            ).message_post(
                message_type='whatsapp',
                **kwargs,
                **{
                    'email_layout_xmlid': 'mail.mail_notification_layout_with_responsible_signature',
                    'email_add_signature': not mail_template,
                    'mail_auto_delete': mail_template.auto_delete,
                    'mail_server_id': mail_template.mail_server_id.id,
                    'reply_to_force_new': False,
                },
            )
        
        # 'body': att['datas'].split(",")[0],
        # 'filename': att['filename'],
        mimetype = guess_mimetype(base64.b64decode(attachment_id.datas))
        if mimetype == 'application/octet-stream':
            mimetype = 'video/mp4'
        str_mimetype = 'data:' + mimetype + ';base64,'
        attachment_datas = str_mimetype + str(attachment_id.datas.decode("utf-8"))
        # print ('==kwargs==',attachment_id.name,attachment_id.datas[:15],attachment_datas[:15])
        partner = move.partner_id
        whatsapp = partner._formatting_mobile_number()
        origin = move.name
        amount_total = move.amount_total
        currency_id = move.currency_id
        link = ''
        status = 'pending'
        # composer.message = html2text.html2text(self.template_id.body_html) if self.template_id.body_html else 'Sample Message'
        body_text = html2text.html2text(new_message.body.replace('_PARTNER_', partner.name).replace('_NUMBER_', origin).replace('_AMOUNT_TOTAL_', str(self.format_amount(amount_total, currency_id)) if currency_id else '').replace('\xa0', ' '))
        message_data = {
            'method': 'sendMessage',
            'phone': whatsapp,
            'chatId': partner.chat_id or '',
            'body': body_text,
            'origin': origin,
            'link': link,
        }
        if partner.whatsapp == '0' and partner.chat_id:
            message_data.update({'phone': '','chatId': partner.chat_id})
        if attachment_datas:
            message_data.update({
                'method': 'sendFile',
                'phone': whatsapp,
                'chatId': partner.chat_id or '',
                'body': body_text,
                'filename': origin.replace(' ','_').replace('-','_'),
                'caption': body_text,
                'origin': origin,
                'link': link,
            })
        
        new_message.whatsapp_data = message_data
        new_message.whatsapp_method = message_data['method']
        new_message.whatsapp_chat_id = partner.chat_id
        new_message.whatsapp_status = status
        # Prevent duplicated attachments linked to the invoice.
        new_message.attachment_ids.write({
            'res_model': new_message._name,
            'res_id': new_message.id,
        })
    
    @api.model
    def _get_whatsapp_params(self, move, move_data):
        #FOR WHATSAPP
        # We must ensure the newly created PDF are added. At this point, the PDF has been generated but not added
        # to 'mail_attachments_widget'.
        mail_attachments_widget = move_data.get('mail_attachments_widget')
        seen_attachment_ids = set()
        to_exclude = {x['name'] for x in mail_attachments_widget if x.get('skip')}
        for attachment_data in self._get_invoice_extra_attachments_data(move) + mail_attachments_widget:
            if attachment_data['name'] in to_exclude:
                continue

            try:
                attachment_id = int(attachment_data['id'])
            except ValueError:
                continue

            seen_attachment_ids.add(attachment_id)

        mail_attachments = [
            (attachment.name, attachment.raw)
            for attachment in self.env['ir.attachment'].browse(list(seen_attachment_ids)).exists()
        ]
        return {
            'body': move_data['mail_body'],
            'subject': move_data['mail_subject'],
            'partner_ids': move_data['mail_partner_ids'].ids,
            'attachments': mail_attachments,
        }, self.env['ir.attachment'].browse(list(seen_attachment_ids)).exists()

    @api.model
    def _send_whatsapps(self, moves_data):
        subtype = self.env.ref('mail.mt_comment')

        for move, move_data in [(move, move_data) for move, move_data in moves_data.items() if move.partner_id.email]:
            mail_template = move_data['mail_template_id']
            mail_lang = move_data['mail_lang']
            mail_params, attachment_id = self._get_whatsapp_params(move, move_data)
            
            if not mail_params:
                continue

            if move_data.get('proforma_pdf_attachment'):
                attachment = move_data['proforma_pdf_attachment']
                mail_params['attachments'].append((attachment.name, attachment.raw))

            email_from = self._get_mail_default_field_value_from_template(mail_template, mail_lang, move, 'email_from')
            model_description = move.with_context(lang=mail_lang).type_name

            self._send_whatsapp(
                move,
                mail_template,
                attachment_id,
                subtype_id=subtype.id,
                model_description=model_description,
                email_from=email_from,
                **mail_params,
            )

    @api.model
    def _process_send_and_print(self, moves, wizard=None, allow_fallback_pdf=False, **kwargs):
        from_cron = not wizard
        
        moves_data = {
            move: {
                **(move.send_and_print_values if not wizard else wizard._get_wizard_values()),
                **self._get_mail_move_values(move, wizard),
            }
            for move in moves
        }
        # Send whatsapp.
        success = {move: move_data for move, move_data in moves_data.items() if not move_data.get('error')}
        if success:
            self._hook_if_whatsapp_success(success, from_cron=from_cron, allow_fallback_pdf=allow_fallback_pdf)
            
        return super()._process_send_and_print(moves, wizard=wizard, allow_fallback_pdf=allow_fallback_pdf, **kwargs)
