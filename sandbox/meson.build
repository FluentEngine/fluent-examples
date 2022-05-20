fluent = subproject('fluent')
ft_log_dep = fluent.get_variable('ft_log_dep')
ft_os_dep = fluent.get_variable('ft_os_dep')
ft_renderer_dep = fluent.get_variable('ft_renderer_dep')

executable('sandbox', 'main.c',
    dependencies	: [ ft_log_dep, ft_renderer_dep, ft_os_dep ],
	install			: true
)
