from odoo import models, fields, api, _


class ProjectProject(models.Model):
    _inherit = "project.project"

    assign_level = fields.One2many(comodel_name="project.assign.level",inverse_name="project_id", string="Assign Level")
