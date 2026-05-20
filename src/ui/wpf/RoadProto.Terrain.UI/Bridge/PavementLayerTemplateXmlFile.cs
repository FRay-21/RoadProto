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
            TemplateName = ReadRequiredString(properties, "name"),
            DisplayScale = ReadRequiredPositiveDouble(properties, "displayScale"),
            PreviewWidth = ReadRequiredPositiveDouble(properties, "previewWidth"),
        };

        foreach (var element in root.Elements("Layer"))
        {
            var type = ReadRequiredEnum<PavementLayerType>(element, "type");
            var thickness = ReadRequiredPositiveDouble(element, "thickness");
            template.Layers.Add(new PavementLayerTemplateLayerDto
            {
                Type = type,
                Name = ReadRequiredString(element, "name"),
                UniformThickness = ReadRequiredBool(element, "uniformThickness"),
                Thickness = thickness,
                InnerThickness = ReadRequiredPositiveDouble(element, "innerThickness"),
                OuterThickness = ReadRequiredPositiveDouble(element, "outerThickness"),
                InnerWidening = ReadRequiredNonNegativeDouble(element, "innerWidening"),
                OuterWidening = ReadRequiredNonNegativeDouble(element, "outerWidening"),
                InnerSlope = ReadRequiredDouble(element, "innerSlope"),
                OuterSlope = ReadRequiredDouble(element, "outerSlope"),
            });
        }

        if (template.Layers.Count == 0)
        {
            throw new InvalidDataException("Pavement layer template XML requires at least one Layer.");
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

    private static string ReadRequiredString(XElement element, string name)
    {
        var value = (string?)element.Attribute(name);
        if (string.IsNullOrWhiteSpace(value))
        {
            throw new InvalidDataException($"Missing pavement layer template XML attribute: {name}.");
        }
        return value!;
    }

    private static double ReadRequiredDouble(XElement element, string name)
    {
        var raw = ReadRequiredString(element, name);
        if (!double.TryParse(raw, NumberStyles.Float, CultureInfo.InvariantCulture, out var value)
            || double.IsNaN(value)
            || double.IsInfinity(value))
        {
            throw new InvalidDataException($"Invalid pavement layer template XML numeric attribute: {name}.");
        }
        return value;
    }

    private static double ReadRequiredPositiveDouble(XElement element, string name)
    {
        var value = ReadRequiredDouble(element, name);
        if (value <= 0.0)
        {
            throw new InvalidDataException($"Pavement layer template XML attribute must be positive: {name}.");
        }
        return value;
    }

    private static double ReadRequiredNonNegativeDouble(XElement element, string name)
    {
        var value = ReadRequiredDouble(element, name);
        if (value < 0.0)
        {
            throw new InvalidDataException($"Pavement layer template XML attribute must be non-negative: {name}.");
        }
        return value;
    }

    private static bool ReadRequiredBool(XElement element, string name)
    {
        var value = ReadRequiredString(element, name);
        if (value.Equals("1", StringComparison.OrdinalIgnoreCase)
            || value.Equals("true", StringComparison.OrdinalIgnoreCase))
        {
            return true;
        }
        if (value.Equals("0", StringComparison.OrdinalIgnoreCase)
            || value.Equals("false", StringComparison.OrdinalIgnoreCase))
        {
            return false;
        }
        throw new InvalidDataException($"Invalid pavement layer template XML boolean attribute: {name}.");
    }

    private static T ReadRequiredEnum<T>(XElement element, string name)
        where T : struct
    {
        var value = ReadRequiredString(element, name);
        if (!Enum.TryParse<T>(value, false, out var result) || !Enum.IsDefined(typeof(T), result))
        {
            throw new InvalidDataException($"Invalid pavement layer template XML enum attribute: {name}.");
        }
        return result;
    }

    private static string Format(double value)
        => value.ToString("R", CultureInfo.InvariantCulture);
}
