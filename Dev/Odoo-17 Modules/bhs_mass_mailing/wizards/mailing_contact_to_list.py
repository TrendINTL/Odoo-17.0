# -*- coding: utf-8 -*-
# Part of Odoo. See LICENSE file for full copyright and licensing details.

from odoo import fields, models, _


class BHSMailingContactToList(models.TransientModel):
    _inherit = 'mailing.contact.to.list'

    def action_add_contacts_and_remove_from_old_list(self):
        """
        Add contacts to the mailing list and remove they from old list.
        With email contacted, do nothing
        """

        list_contact = []

        # get list contacted
        dict_contacted = self.env['mailing.contact.contacted'].sudo().search_read([], ['email'])    # [{'email': 'x'}, ...]
        update_lists_mode = self.env.context.get('update_lists_mode')
        list_contacted = [item['email'] for item in dict_contacted] if not update_lists_mode else []

        # Remove from old list
        for contact in self.contact_ids:
            if not (contact.email in list_contacted):
                list_contact.append(contact.id)
                if contact.subscription_ids:
                    for sub in contact.subscription_ids:
                        sub.unlink()

        if list_contact == []:
            return
        # Add email not contacted to new list
        self.contact_ids = list_contact
        return self._add_contacts_to_mailing_list({'type': 'ir.actions.act_window_close'})