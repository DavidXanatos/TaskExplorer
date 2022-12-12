################################################################
# Qwt Widget Library
# Copyright (C) 1997   Josef Wilgen
# Copyright (C) 2002   Uwe Rathmann
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the Qwt License, Version 1.0
################################################################


HEADERS += \
    qwt.h \
    qwt_abstract_scale_draw.h \
    qwt_bezier.h \
    qwt_clipper.h \
    qwt_color_map.h \
    qwt_column_symbol.h \
    qwt_date.h \
    qwt_date_scale_draw.h \
    qwt_date_scale_engine.h \
    qwt_dyngrid_layout.h \
    qwt_global.h \
    qwt_graphic.h \
    qwt_interval.h \
    qwt_interval_symbol.h \
    qwt_math.h \
    qwt_magnifier.h \
    qwt_null_paintdevice.h \
    qwt_painter.h \
    qwt_painter_command.h \
    qwt_panner.h \
    qwt_picker.h \
    qwt_picker_machine.h \
    qwt_pixel_matrix.h \
    qwt_point_3d.h \
    qwt_point_polar.h \
    qwt_round_scale_draw.h \
    qwt_scale_div.h \
    qwt_scale_draw.h \
    qwt_scale_engine.h \
    qwt_scale_map.h \
    qwt_spline.h \
    qwt_spline_basis.h \
    qwt_spline_parametrization.h \
    qwt_spline_local.h \
    qwt_spline_cubic.h \
    qwt_spline_pleasing.h \
    qwt_spline_polynomial.h \
    qwt_symbol.h \
    qwt_system_clock.h \
    qwt_text_engine.h \
    qwt_text_label.h \
    qwt_text.h \
    qwt_transform.h \
    qwt_widget_overlay.h

SOURCES += \
    qwt.cpp \
    qwt_abstract_scale_draw.cpp \
    qwt_bezier.cpp \
    qwt_clipper.cpp \
    qwt_color_map.cpp \
    qwt_column_symbol.cpp \
    qwt_date.cpp \
    qwt_date_scale_draw.cpp \
    qwt_date_scale_engine.cpp \
    qwt_dyngrid_layout.cpp \
    qwt_event_pattern.cpp \
    qwt_graphic.cpp \
    qwt_interval.cpp \
    qwt_interval_symbol.cpp \
    qwt_math.cpp \
    qwt_magnifier.cpp \
    qwt_null_paintdevice.cpp \
    qwt_painter.cpp \
    qwt_painter_command.cpp \
    qwt_panner.cpp \
    qwt_picker.cpp \
    qwt_picker_machine.cpp \
    qwt_pixel_matrix.cpp \
    qwt_point_3d.cpp \
    qwt_point_polar.cpp \
    qwt_round_scale_draw.cpp \
    qwt_scale_div.cpp \
    qwt_scale_draw.cpp \
    qwt_scale_map.cpp \
    qwt_scale_engine.cpp \
    qwt_spline.cpp \
    qwt_spline_basis.cpp \
    qwt_spline_parametrization.cpp \
    qwt_spline_local.cpp \
    qwt_spline_cubic.cpp \
    qwt_spline_pleasing.cpp \
    qwt_spline_polynomial.cpp \
    qwt_symbol.cpp \
    qwt_system_clock.cpp \
    qwt_text_engine.cpp \
    qwt_text_label.cpp \
    qwt_text.cpp \
    qwt_transform.cpp \
    qwt_widget_overlay.cpp

 
contains(QWT_CONFIG, QwtPlot) {

    HEADERS += \
        qwt_axis.h \
        qwt_axis_id.h \
        qwt_curve_fitter.h \
        qwt_spline_curve_fitter.h \
        qwt_weeding_curve_fitter.h \
        qwt_event_pattern.h \
        qwt_abstract_legend.h \
        qwt_legend.h \
        qwt_legend_data.h \
        qwt_legend_label.h \
        qwt_plot.h \
        qwt_plot_renderer.h \
        qwt_plot_curve.h \
        qwt_plot_dict.h \
        qwt_plot_directpainter.h \
        qwt_plot_graphicitem.h \
        qwt_plot_grid.h \
        qwt_plot_histogram.h \
        qwt_plot_item.h \
        qwt_plot_abstract_barchart.h \
        qwt_plot_barchart.h \
        qwt_plot_multi_barchart.h \
        qwt_plot_intervalcurve.h \
        qwt_plot_tradingcurve.h \
        qwt_plot_layout.h \
        qwt_plot_marker.h \
        qwt_plot_zoneitem.h \
        qwt_plot_textlabel.h \
        qwt_plot_rasteritem.h \
        qwt_plot_spectrogram.h \
        qwt_plot_spectrocurve.h \
        qwt_plot_scaleitem.h \
        qwt_plot_legenditem.h \
        qwt_plot_seriesitem.h \
        qwt_plot_shapeitem.h \
        qwt_plot_vectorfield.h \
        qwt_plot_abstract_canvas.h \
        qwt_plot_canvas.h \
        qwt_plot_panner.h \
        qwt_plot_picker.h \
        qwt_plot_zoomer.h \
        qwt_plot_magnifier.h \
        qwt_plot_rescaler.h \
        qwt_point_mapper.h \
        qwt_raster_data.h \
        qwt_matrix_raster_data.h \
        qwt_vectorfield_symbol.h \
        qwt_sampling_thread.h \
        qwt_samples.h \
        qwt_series_data.h \
        qwt_series_store.h \
        qwt_point_data.h \
        qwt_scale_widget.h 

    SOURCES += \
        qwt_curve_fitter.cpp \
        qwt_spline_curve_fitter.cpp \
        qwt_weeding_curve_fitter.cpp \
        qwt_abstract_legend.cpp \
        qwt_legend.cpp \
        qwt_legend_data.cpp \
        qwt_legend_label.cpp \
        qwt_plot.cpp \
        qwt_plot_renderer.cpp \
        qwt_plot_axis.cpp \
        qwt_plot_curve.cpp \
        qwt_plot_dict.cpp \
        qwt_plot_directpainter.cpp \
        qwt_plot_graphicitem.cpp \
        qwt_plot_grid.cpp \
        qwt_plot_histogram.cpp \
        qwt_plot_item.cpp \
        qwt_plot_abstract_barchart.cpp \
        qwt_plot_barchart.cpp \
        qwt_plot_multi_barchart.cpp \
        qwt_plot_intervalcurve.cpp \
        qwt_plot_zoneitem.cpp \
        qwt_plot_tradingcurve.cpp \
        qwt_plot_spectrogram.cpp \
        qwt_plot_spectrocurve.cpp \
        qwt_plot_scaleitem.cpp \
        qwt_plot_legenditem.cpp \
        qwt_plot_seriesitem.cpp \
        qwt_plot_shapeitem.cpp \
        qwt_plot_vectorfield.cpp \
        qwt_plot_marker.cpp \
        qwt_plot_textlabel.cpp \
        qwt_plot_layout.cpp \
        qwt_plot_abstract_canvas.cpp \
        qwt_plot_canvas.cpp \
        qwt_plot_panner.cpp \
        qwt_plot_rasteritem.cpp \
        qwt_plot_picker.cpp \
        qwt_plot_zoomer.cpp \
        qwt_plot_magnifier.cpp \
        qwt_plot_rescaler.cpp \
        qwt_point_mapper.cpp \
        qwt_raster_data.cpp \
        qwt_matrix_raster_data.cpp \
        qwt_vectorfield_symbol.cpp \
        qwt_sampling_thread.cpp \
        qwt_series_data.cpp \
        qwt_point_data.cpp \
        qwt_scale_widget.cpp

    contains(QWT_CONFIG, QwtOpenGL) {

        lessThan(QT_MAJOR_VERSION, 6) {

            HEADERS += \
                qwt_plot_glcanvas.h

            SOURCES += \
                qwt_plot_glcanvas.cpp
        }

        greaterThan(QT_MAJOR_VERSION, 4) {

            lessThan( QT_MAJOR_VERSION, 6) {

                greaterThan(QT_MINOR_VERSION, 3) {

                    HEADERS += qwt_plot_opengl_canvas.h
                    SOURCES += qwt_plot_opengl_canvas.cpp
                }
            }
            else {
                QT += openglwidgets

                HEADERS += qwt_plot_opengl_canvas.h
                SOURCES += qwt_plot_opengl_canvas.cpp
            }
            
        }

    }

    contains(QWT_CONFIG, QwtSvg) {

        HEADERS += \
            qwt_plot_svgitem.h

        SOURCES += \
            qwt_plot_svgitem.cpp
    }

    contains(QWT_CONFIG, QwtPolar) {

        HEADERS += \
            qwt_polar.h \
            qwt_polar_canvas.h \
            qwt_polar_curve.h \
            qwt_polar_fitter.h \
            qwt_polar_grid.h \
            qwt_polar_itemdict.h \
            qwt_polar_item.h \
            qwt_polar_layout.h \
            qwt_polar_magnifier.h \
            qwt_polar_marker.h \
            qwt_polar_panner.h \
            qwt_polar_picker.h \
            qwt_polar_plot.h \
            qwt_polar_renderer.h \
            qwt_polar_spectrogram.h

        SOURCES += \
            qwt_polar_canvas.cpp \
            qwt_polar_curve.cpp \
            qwt_polar_fitter.cpp \
            qwt_polar_grid.cpp \
            qwt_polar_item.cpp \
            qwt_polar_itemdict.cpp \
            qwt_polar_layout.cpp \
            qwt_polar_magnifier.cpp \
            qwt_polar_marker.cpp \
            qwt_polar_panner.cpp \
            qwt_polar_picker.cpp \
            qwt_polar_plot.cpp \
            qwt_polar_renderer.cpp \
            qwt_polar_spectrogram.cpp
    }
}

greaterThan(QT_MAJOR_VERSION, 4) {

    QT += printsupport
    QT += concurrent
} 

contains(QWT_CONFIG, QwtSvg) {

    greaterThan(QT_MAJOR_VERSION, 4) {

        qtHaveModule(svg) {
            QT += svg
        }
        else {
            warning("QwtSvg is enabled in qwtconfig.pri, but Qt has not been built with svg support")
        }
    }
    else {
        QT += svg
    }
}
else {

    DEFINES += QWT_NO_SVG
}

contains(QWT_CONFIG, QwtOpenGL) {

   greaterThan(QT_MAJOR_VERSION, 4) {

        qtHaveModule(opengl) {
            QT += opengl
        }
        else {
            warning("QwtOpenGL is enabled in qwtconfig.pri, but Qt has not been built with opengl support")
        }
    }
    else {
        QT += opengl
    }

    QT += opengl
}
else {

    DEFINES += QWT_NO_OPENGL
}

contains(QWT_CONFIG, QwtWidgets) {

    HEADERS += \
        qwt_abstract_slider.h \
        qwt_abstract_scale.h \
        qwt_arrow_button.h \
        qwt_analog_clock.h \
        qwt_compass.h \
        qwt_compass_rose.h \
        qwt_counter.h \
        qwt_dial.h \
        qwt_dial_needle.h \
        qwt_knob.h \
        qwt_slider.h \
        qwt_thermo.h \
        qwt_wheel.h
    
    SOURCES += \
        qwt_abstract_slider.cpp \
        qwt_abstract_scale.cpp \
        qwt_arrow_button.cpp \
        qwt_analog_clock.cpp \
        qwt_compass.cpp \
        qwt_compass_rose.cpp \
        qwt_counter.cpp \
        qwt_dial.cpp \
        qwt_dial_needle.cpp \
        qwt_knob.cpp \
        qwt_slider.cpp \
        qwt_thermo.cpp \
        qwt_wheel.cpp
}
