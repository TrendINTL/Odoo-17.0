from odoo import fields, models, api


class ProjectAssignLevel(models.Model):
    _name = "project.assign.level"
    _description = "Project Assign Level"

    project_id = fields.Many2one(comodel_name="project.project", string="Project")
    stage_id = fields.Many2one(comodel_name="project.task.type", string="Stage")
    user_id = fields.Many2one(comodel_name="res.users", string="User")
