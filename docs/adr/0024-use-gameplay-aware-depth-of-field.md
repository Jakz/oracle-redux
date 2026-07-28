# Use gameplay-aware depth-of-field

Status: Accepted

Modern depth-of-field combines presentation depth with a semantic focus mask
rather than blurring every world pixel solely by physical distance. Focus
normally follows Link's ground-contact position. Link, nearby interactable
actors, important pickups, and gameplay-critical effects can be protected by
Presentation Metadata; cutscenes may explicitly override the focus target.
Interface and dialogue passes are never blurred, World Overview disables
depth-of-field by default, and users can independently reduce or disable the
effect. Focus data affects only presentation and cannot change activation,
interaction, collision, targeting, or any other simulation rule.
