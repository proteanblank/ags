using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Linq;
using System.Reflection;
using System.Text;
using System.Threading.Tasks;

namespace AGS.Types
{
    /// <summary>
    /// InteractionPropertyDescriptor defines a real property from
    /// Interactions type, wrapped and depicted as being a property of
    /// another type instead, such as e.g. Character type.
    /// This allows to have this pseudo-property displayed on that type's
    /// Events properties tab.
    /// </summary>
    public class InteractionPropertyDescriptor : PropertyDescriptor
    {
        private Type _componentType;
        private bool _isReadonly;

        public InteractionPropertyDescriptor(object component, string propertyName, Attribute[] attributes, bool isReadOnly)
            : base(propertyName, attributes)
        {
            _componentType = component.GetType();
            _isReadonly = isReadOnly;
        }

        public override bool CanResetValue(object component)
        {
            return false;
        }

        public override Type ComponentType
        {
            get { return _componentType; }
        }

        public override object GetValue(object component)
        {
            PropertyInfo interactionsProperty = component.GetType().GetProperty("Interactions");
            Interactions interactions = (Interactions)interactionsProperty.GetValue(component, null);
            var valueProperty = typeof(Interactions).GetProperty(Name);
            return valueProperty.GetValue(interactions);
        }

        public override bool IsReadOnly
        {
            get { return _isReadonly; }
        }

        public override Type PropertyType
        {
            get { return typeof(string); }
        }

        public override void ResetValue(object component)
        {
            throw new Exception("The method or operation is not implemented.");
        }

        public override void SetValue(object component, object value)
        {
            if (_isReadonly)
                return;

            PropertyInfo interactionsProperty = component.GetType().GetProperty("Interactions");
            Interactions interactions = (Interactions)interactionsProperty.GetValue(component, null);
            var valueProperty = typeof(Interactions).GetProperty(Name);
            valueProperty.SetValue(interactions, value);
        }

        public override bool ShouldSerializeValue(object component)
        {
            return false;
        }
    }
}
