from odoo import models, fields, api, _


class ProjectTask(models.Model):
    _inherit = "project.task"

    @api.model
    def create(self, vals):
        res = super(ProjectTask, self).create(vals)
        if 'stage_id' in vals:
            stage_id = self.env["project.task.type"].search([("id", "=", vals["stage_id"])])
            assign_id = res.project_id.assign_level.filtered(lambda a: a.stage_id.id == stage_id.id)
            if assign_id:
                res.user_ids = [(4, assign_id.user_id.id)]
        return res


    def write(self, vals):
        if 'stage_id' in vals:
            stage_id = self.env["project.task.type"].search([("id", "=", vals["stage_id"])])
            assign_id = self.project_id.assign_level.filtered(lambda a: a.stage_id.id == self.stage_id.id and a.user_id.id in self.user_ids.ids)
            if assign_id:
                self.user_ids = [(3, assign_id.user_id.id, 0)]
        res = super(ProjectTask, self).write(vals)
        if 'stage_id' in vals:
            stage_id = self.env["project.task.type"].search([("id", "=", vals["stage_id"])])
            assign_id = self.project_id.assign_level.filtered(lambda a: a.stage_id.id == stage_id.id and a.user_id.id not in self.user_ids.ids)
            if assign_id:
                self.user_ids = [(4, assign_id.user_id.id)]
        return res