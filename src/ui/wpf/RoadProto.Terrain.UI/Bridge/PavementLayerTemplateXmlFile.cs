using System;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Xml.Linq;

namespace RoadProto.Terrain.UI.Bridge;

public static class PavementLayerTemplateXmlFile
{
    public const string Extension = ".rpavement.xml";

    public static PavementLayerTemplateDto Read(string path)
    {
        ValidateExtension(path);
        var document = XDocument.Load(path);
        var root = document.Root ?? throw new InvalidDataException("Pavement layer template XML is empty.");
        if (root.Name != "RoadProtoPavementLayerTemplate" || (string?)root.Attribute("version") != "1")
        {
            throw new InvalidDataException("Unsupported pavement layer template XML root.");
        }

        var properties = root.Element("Properties") ?? throw new InvalidDataException("Missing pavement layer template properties.");
        var template = new PavementLayerTemplateDto
        {
            TemplateName = (string?)properties.Attribute("name") ?? "路面结构层模板",
            DisplayScale = ReadDouble(properties, "displayScale", 10.0),
            PreviewWidth = ReadDouble(properties, "previewWidth", 3.75),
        };

        foreach (var element in root.Elements("Layer"))
        {
            var type = ReadEnum((string?)element.Attribute("type"), PavementLayerType.UpperSurface);
            var thickness = ReadDouble(element, "thickness", 0.04);
            template.Layers.Add(new PavementLayerTemplateLayerDto
            {
                Type = type,
                Name = (string?)element.Attribute("name") ?? PavementLayerTemplateLabels.LayerTypeLabel(type),
                UniformThickness = ReadBool(element, "uniformThickness", true),
                Thickness = thickness,
                InnerThickness = ReadDouble(element, "innerThickness", thickness),
                OuterThickness = ReadDouble(element, "outerThickness", thickness),
                InnerWidening = ReadDouble(element, "innerWidening"),
                OuterWidening = ReadDouble(element, "outerWidening"),
                InnerSlope = ReadDouble(element, "innerSlope"),
                OuterSlope = ReadDouble(element, "outerSlope"),
            });
        }

        if (template.Layers.Count == 0)
        {
            template.Layers.AddRange(PavementLayerTemplateLabels.DefaultLayers());
        }

        return template;
    }

    public static void Write(string path, PavementLayerTemplateDto template)
    {
        ValidateExtension(path);
        var document = new XDocument(
            new XElement(
                "RoadProtoPavementLayerTemplate",
                new XAttribute("version", "1"),
                new XElement(
                    "Properties",
                    new XAttribute("name", template.TemplateName ?? string.Empty),
                    new XAttribute("displayScale", Format(template.DisplayScale)),
                    new XAttribute("previewWidth", Format(template.PreviewWidth))),
                template.Layers.Select(layer => new XElement(
                    "Layer",
                    new XAttribute("type", layer.Type.ToString()),
                    new XAttribute("name", layer.Name ?? string.Empty),
                    new XAttribute("uniformThickness", layer.UniformThickness ? "true" : "false"),
                    new XAttribute("thickness", Format(layer.Thickness)),
                    new XAttribute("innerThickness", Format(layer.InnerThickness)),
                    new XAttribute("outerThickness", Format(layer.OuterThickness)),
                    new XAttribute("innerWidening", Format(layer.InnerWidening)),
                    new XAttribute("outerWidening", Format(layer.OuterWidening)),
                    new XAttribute("innerSlope", Format(layer.InnerSlope)),
                    new XAttribute("outerSlope", Format(layer.OuterSlope))))));

        document.Save(path);
    }

    private static void ValidateExtension(string path)
    {
        if (!path.EndsWith(Extension, StringComparison.OrdinalIgnoreCase))
        {
            throw new InvalidDataException("Pavement layer template XML file must use .rpavement.xml suffix.");
        }
    }

    private static double ReadDouble(XElement element, string name, double fallback = 0.0)
        => double.TryParse((string?)element.Attribute(name), NumberStyles.Float, CultureInfo.InvariantCulture, out var value)
            ? value
            : fallback;

    private static bool ReadBool(XElement element, string name, bool fallback = false)
    {
        var value = (string?)element.Attribute(name);
        if (string.IsNullOrWhiteSpace(value))
        {
            return fallback;
        }
        return value!.Equals("1", StringComparison.OrdinalIgnoreCase)
            || value.Equals("true", StringComparison.OrdinalIgnoreCase);
    }

    private static T ReadEnum<T>(string? value, T fallback)
        where T : struct
        => string.IsNullOrWhiteSpace(value)
            ? fallback
            : Enum.TryParse<T>(value, true, out var result) ? result : fallback;

    private static string Format(double value)
        => value.ToString("R", CultureInfo.InvariantCulture);
}
